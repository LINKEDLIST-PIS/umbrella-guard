// 테스트: microSD 에 파일 쓰고 다시 읽기
//
// 초기화에 실패하면 SPI 속도를 4 MHz -> 1 MHz -> 250 kHz 로 낮춰 가며
// 2 초마다 계속 재시도한다. 리셋 없이 배선/카드를 고쳐 가며 확인 가능.
//
// SdError 첫 값 해석 (주요한 것만)
//   0x01 (CMD0)   : 카드 무응답 -> 배선, 전원, 카드 삽입, 모듈 종류 문제. 포맷과 무관
//   0x08 (CMD8) 등: 응답은 있으나 통신 불안정 -> 선 길이, 속도, 접촉
//   init OK 인데 볼륨 실패 : 포맷 문제 -> FAT32 로 포맷
#include <Arduino.h>
#include <SdFat.h>
#include "pins.h"

// Uno 는 FAT16/32 전용 (exFAT 은 RAM 부족). 볼륨과 파일 타입을 반드시 짝 맞출 것:
// SdFat32 <-> File32. (SdFat + FsFile 조합은 컴파일은 되지만 open 이 항상 실패)
SdFat32 sd;
File32 file;

uint8_t attempt = 0;
bool done = false;

static bool tryBegin(uint32_t hz) {
  Serial.print(F("SD init @ "));
  Serial.print(hz / 1000);
  Serial.print(F(" kHz ... "));
  if (!sd.cardBegin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, hz))) {
    Serial.print(F("카드 무응답, SdError=0x"));
    Serial.print(sd.sdErrorCode(), HEX);
    Serial.print(F(",0x"));
    Serial.println(sd.sdErrorData(), HEX);
    return false;
  }
  Serial.println(F("카드 응답 OK"));

  Serial.print(F("카드 용량: "));
  Serial.print((uint32_t)(sd.card()->sectorCount() / 2048));
  Serial.println(F(" MB"));

  if (!sd.volumeBegin()) {
    Serial.println(F("볼륨 마운트 실패 -> 카드를 FAT32 로 포맷하세요"));
    return false;
  }
  Serial.print(F("파일시스템: FAT"));
  Serial.println(sd.fatType());
  return true;
}

static void writeAndRead() {
  if (!file.open("TEST.TXT", O_WRONLY | O_CREAT | O_APPEND)) {
    Serial.println(F("파일 열기 실패"));
    return;
  }
  file.print(F("hello umbrella, millis="));
  file.println(millis());
  file.close();
  Serial.println(F("TEST.TXT 에 한 줄 추가함. 내용:"));

  if (file.open("TEST.TXT", O_RDONLY)) {
    while (file.available()) Serial.write(file.read());
    file.close();
  }
  Serial.println(F("[test_sd] 완료"));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println(F("[test_sd] start"));
}

void loop() {
  if (done) return;
  static const uint32_t speeds[] = {4000000UL, 1000000UL, 250000UL};
  uint32_t hz = speeds[attempt % 3];
  attempt++;
  if (tryBegin(hz)) {
    writeAndRead();
    done = true;
    return;
  }
  sd.end();
  delay(2000);
}
