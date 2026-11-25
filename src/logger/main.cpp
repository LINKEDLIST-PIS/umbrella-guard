// env:logger - MPU-6050 값을 microSD 에 CSV 로 기록 (실험/임계값 튜닝용)
//
// - 부팅 시 LOG00.CSV ~ LOG99.CSV 중 비어 있는 이름으로 새 파일 생성
//   (시리얼 모니터를 열면 Uno 가 리셋되므로 그때마다 새 파일이 생김)
// - 50 Hz 로 기록, 1 초마다 sync (전원을 갑자기 꺼도 최대 1 초만 손실)
//
// 시리얼 명령 (한 글자)
//   0~9 : mark 값 변경 (실험 구간 표시. 예: 0=가만히, 1=꺼냄, 2=부딪힘, 3=진동)
//   v   : 실시간 보기 on/off (10 Hz 로 시리얼에 출력, 기록은 50 Hz 그대로)
//   s   : 기록 종료 (파일 닫기. 카드 빼기 전에 권장)
//   l   : 카드의 파일 목록과 크기
//   d   : 현재 파일 내용을 시리얼로 출력 (기록 중이면 먼저 종료)
//   p   : 직전 파일(현재 번호 - 1) 내용을 시리얼로 출력 (기록 중이면 먼저 종료)
//   n   : 새 파일로 기록 재시작
#include <Arduino.h>
#include <Wire.h>
#include <SdFat.h>
#include <MPU6050.h>
#include "pins.h"
#include "mpu_check.h"
#include "sd_init.h"

constexpr uint16_t SAMPLE_PERIOD_MS = 20;   // 50 Hz
constexpr uint16_t SYNC_PERIOD_MS   = 1000;
constexpr uint8_t  VIEW_EVERY       = 5;    // 실시간 보기: 5 샘플마다 1 줄 (10 Hz)

MPU6050 mpu(MPU_I2C_ADDR);
// Uno 는 FAT16/32 전용. 볼륨과 파일 타입 짝: SdFat32 <-> File32
SdFat32 sd;
File32 logFile;

char fileName[] = "LOG00.CSV";
bool logging = false;
bool liveView = false;
uint8_t mark = 0;
uint8_t viewCount = 0;
uint32_t nextSample = 0;
uint32_t lastSync = 0;
uint32_t rows = 0;

static void halt(const __FlashStringHelper* msg) {
  Serial.println(msg);
  while (true) {}
}

static bool startNewFile() {
  for (uint8_t i = 0; i < 100; i++) {
    fileName[3] = '0' + i / 10;
    fileName[4] = '0' + i % 10;
    if (!sd.exists(fileName)) {
      if (!logFile.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {
        Serial.println(F("파일 생성 실패"));
        return false;
      }
      // 기본 범위: 가속도 +-2g (16384 LSB/g), 자이로 +-250 dps (131 LSB/dps)
      logFile.println(F("ms,ax,ay,az,gx,gy,gz,mark"));
      logging = true;
      rows = 0;
      Serial.print(F("기록 중: "));
      Serial.println(fileName);
      nextSample = millis();
      lastSync = nextSample;
      return true;
    }
  }
  Serial.println(F("파일 이름 고갈 (LOG99 까지 사용됨) - 카드 정리 필요"));
  return false;
}

static void stopLogging() {
  if (!logging) return;
  logFile.close();
  logging = false;
  Serial.print(F("기록 종료: "));
  Serial.print(fileName);
  Serial.print(F(", "));
  Serial.print(rows);
  Serial.print(F(" 행 ("));
  Serial.print(rows / 50);
  Serial.println(F(" 초). 카드 제거 가능"));
}

static void dumpFile(const char* name) {
  stopLogging();
  File32 f;
  if (!f.open(name, O_RDONLY)) {
    Serial.print(F("파일 열기 실패: "));
    Serial.println(name);
    return;
  }
  Serial.print(F("----- "));
  Serial.print(name);
  Serial.println(F(" 시작 -----"));
  uint8_t buf[64];
  int n;
  while ((n = f.read(buf, sizeof(buf))) > 0) Serial.write(buf, n);
  f.close();
  Serial.println(F("----- 끝 -----"));
}

static void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c >= '0' && c <= '9') {
      mark = c - '0';
      Serial.print(F("mark=")); Serial.println(mark);
    } else if (c == 'v') {
      liveView = !liveView;
      Serial.println(liveView ? F("실시간 보기 ON (ms,ax,ay,az,gx,gy,gz,mark)") : F("실시간 보기 OFF"));
    } else if (c == 's') {
      stopLogging();
    } else if (c == 'l') {
      if (logging) logFile.sync();
      Serial.println(F("파일 목록 (크기 bytes):"));
      sd.ls(&Serial, LS_SIZE);
    } else if (c == 'd') {
      dumpFile(fileName);
    } else if (c == 'p') {
      // 직전 파일 (모니터를 다시 열면 리셋되어 새 파일이 생기므로, 이전 실험은 여기로 확인)
      char prev[] = "LOG00.CSV";
      uint8_t idx = (fileName[3] - '0') * 10 + (fileName[4] - '0');
      if (idx == 0) {
        Serial.println(F("직전 파일 없음"));
      } else {
        idx--;
        prev[3] = '0' + idx / 10;
        prev[4] = '0' + idx % 10;
        dumpFile(prev);
      }
    } else if (c == 'n') {
      stopLogging();
      startNewFile();
    }
  }
}

template <typename P>
static void printRow(P& out, uint32_t ms, const int16_t* v) {
  out.print(ms);
  for (uint8_t i = 0; i < 6; i++) {
    out.write(',');
    out.print(v[i]);
  }
  out.write(',');
  out.println(mark);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println(F("[logger] start  (명령: 0~9 mark, v 실시간, s 종료, l 목록, d 현재파일, p 직전파일, n 새파일)"));

  Wire.begin();
  mpu.initialize();
  if (!mpuCheckAndReport()) halt(F("MPU-6050 연결 실패"));

  if (!sdBeginRetry(sd)) halt(F("SD init 실패 (재시도 모두 실패) - 배선/전원 확인"));
  if (!startNewFile()) halt(F("기록 시작 실패"));
}

void loop() {
  handleSerial();
  if (!logging) return;

  uint32_t now = millis();
  if ((int32_t)(now - nextSample) < 0) return;
  nextSample += SAMPLE_PERIOD_MS;

  int16_t v[6];
  mpu.getMotion6(&v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
  printRow(logFile, now, v);
  rows++;

  if (liveView && ++viewCount >= VIEW_EVERY) {
    viewCount = 0;
    printRow(Serial, now, v);
  }

  if (now - lastSync >= SYNC_PERIOD_MS) {
    logFile.sync();
    lastSync = now;
  }
}
