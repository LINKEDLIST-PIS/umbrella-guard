// env:main - 우산 도난 감지 최종 기능 (MPU-6050 + HM-10 + 부저, SD 없음)
//
// 상태
//   DISARMED : 대기. 움직여도 반응 없음
//   SETTLING : ARM 직후 내려놓을 시간 (SETTLE_MS). 마지막 0.5 초 평균을 기준 자세로 저장
//   ARMED    : 보관 모드. 기준 자세에서 벗어나거나 회전이 감지되면 ALARM
//   ALARM    : 부저 울림 + "ALERT:MOVED" 주기 전송. DISARM 명령으로 해제
//
// BLE / 시리얼 프로토콜은 docs/APP_SPEC.md 참고 (PROTO_VERSION 과 함께 관리).
//   앱 -> 기기 : ARM, DISARM, STATUS, PING, INFO   (줄바꿈 또는 100 ms 무입력으로 종료)
//   기기 -> 앱 : STATE:<모드>, OK:<명령>, ALERT:MOVED, PONG, INFO:..., ERR:<코드>  (\r\n 종료)
//
// 임계값은 env:logger 로 수집한 데이터와 tools/analyze_log.py 로 조정할 것.
#include <Arduino.h>
#include <Wire.h>
#include <AltSoftSerial.h>
#include <MPU6050.h>
#include "pins.h"
#include "mpu_check.h"
#include "ble_state.h"

// ---- 버전 (프로토콜을 바꾸면 PROTO_VERSION 을 올리고 docs/APP_SPEC.md 갱신) ----
#define FW_VERSION    "1.0.0"
#define PROTO_VERSION "1"

// ---- 튜닝 값 ----
constexpr uint16_t SAMPLE_PERIOD_MS  = 20;    // 50 Hz
constexpr uint16_t SETTLE_MS         = 3000;  // ARM 후 내려놓을 시간
constexpr uint8_t  BASELINE_SAMPLES  = 25;    // SETTLING 마지막 0.5 초 평균을 기준 자세로
constexpr int32_t  ACC_THRESH        = 2500;  // |a - 기준| 합, 약 0.15 g (16384 LSB/g)
constexpr int32_t  GYRO_THRESH       = 2600;  // |g| 합, 약 20 dps (131 LSB/dps)
constexpr uint8_t  TRIGGER_COUNT     = 3;     // 연속 N 샘플 초과 시 경보 (노이즈 억제)
constexpr uint16_t ALERT_REPEAT_MS   = 2000;  // ALARM 중 ALERT 재전송 주기
constexpr uint16_t BEEP_ON_MS        = 200;
constexpr uint16_t BEEP_OFF_MS       = 200;
constexpr uint16_t CMD_TIMEOUT_MS    = 100;

enum class Mode : uint8_t { DISARMED, SETTLING, ARMED, ALARM };

MPU6050 mpu(MPU_I2C_ADDR);
AltSoftSerial ble;  // RX=D8, TX=D9 고정
BleState bleState;

Mode mode = Mode::DISARMED;
uint32_t modeSince = 0;
uint32_t nextSample = 0;
uint32_t lastAlert = 0;
int16_t baseAx, baseAy, baseAz;
int32_t sumAx, sumAy, sumAz;
uint8_t baseCount = 0;
uint8_t overCount = 0;
bool sensorOk = false;

// ---- 출력: 한 줄을 시리얼과 (연결 시) BLE 로 동시에 보낸다 ----
// prefix 와 value 를 붙여 "STATE:ARMED" 같은 한 줄을 만든다. value 는 생략 가능.
static void send(const __FlashStringHelper* prefix, const __FlashStringHelper* value = nullptr) {
  Serial.print(prefix);
  if (value) Serial.print(value);
  Serial.println();
  if (bleState.connected()) {
    ble.print(prefix);
    if (value) ble.print(value);
    ble.print(F("\r\n"));
  }
}

static const __FlashStringHelper* modeName() {
  switch (mode) {
    case Mode::DISARMED: return F("DISARMED");
    case Mode::SETTLING: return F("SETTLING");
    case Mode::ARMED:    return F("ARMED");
    case Mode::ALARM:    return F("ALARM");
  }
  return F("?");
}

static void sendState() { send(F("STATE:"), modeName()); }

static void setMode(Mode m) {
  mode = m;
  modeSince = millis();
  overCount = 0;
  baseCount = 0;
  sumAx = sumAy = sumAz = 0;
  if (m != Mode::ALARM) digitalWrite(PIN_BUZZER, LOW);
  sendState();
}

// ---- 명령 처리 ----
struct CmdBuf {
  char buf[16];
  uint8_t len = 0;
  uint32_t lastRx = 0;
};
CmdBuf serialCmd, bleCmd;

static void handleCommand(char* s) {
  for (char* p = s; *p; p++) *p = toupper(*p);
  if (s[0] == '\0') return;
  // HM-10 자체 알림 (AT+NOTI1 설정 시 "OK+CONN", "OK+LOST" 등) 은 명령이 아니므로 무시
  if (strncmp(s, "OK+", 3) == 0) return;

  if (strcmp(s, "ARM") == 0) {
    if (!sensorOk) {
      send(F("ERR:SENSOR"));
      return;
    }
    send(F("OK:ARM"));
    if (mode == Mode::DISARMED) setMode(Mode::SETTLING);
    else sendState();  // 이미 무장/경보 중이면 상태만 다시 알림 (멱등)
  } else if (strcmp(s, "DISARM") == 0) {
    send(F("OK:DISARM"));
    setMode(Mode::DISARMED);
  } else if (strcmp(s, "STATUS") == 0) {
    sendState();
  } else if (strcmp(s, "PING") == 0) {
    send(F("PONG"));
  } else if (strcmp(s, "INFO") == 0) {
    send(F("INFO:fw=" FW_VERSION ",proto=" PROTO_VERSION),
         sensorOk ? F(",sensor=ok") : F(",sensor=fail"));
  } else {
    send(F("ERR:UNKNOWN"));
  }
}

static void feed(CmdBuf& c, int ch) {
  c.lastRx = millis();
  if (ch == '\r' || ch == '\n') {
    c.buf[c.len] = '\0';
    handleCommand(c.buf);
    c.len = 0;
  } else if (c.len < sizeof(c.buf) - 1) {
    c.buf[c.len++] = (char)ch;
  }
}

static void flushOnTimeout(CmdBuf& c) {
  if (c.len && millis() - c.lastRx >= CMD_TIMEOUT_MS) feed(c, '\n');
}

static void onBleConnectionChanged() {
  if (bleState.connected()) {
    Serial.println(F("# BLE 연결"));
    sendState();  // 연결되면 현재 상태를 먼저 알림
  } else {
    Serial.println(F("# BLE 해제"));
  }
}

// ---- 동작 감지 ----
static void sampleMotion() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  if (mode == Mode::SETTLING) {
    uint32_t elapsed = millis() - modeSince;
    if (elapsed + (uint32_t)BASELINE_SAMPLES * SAMPLE_PERIOD_MS >= SETTLE_MS) {
      sumAx += ax; sumAy += ay; sumAz += az;
      if (++baseCount >= BASELINE_SAMPLES) {
        baseAx = sumAx / baseCount;
        baseAy = sumAy / baseCount;
        baseAz = sumAz / baseCount;
        setMode(Mode::ARMED);
      }
    }
    return;
  }
  if (mode != Mode::ARMED) return;

  int32_t dAcc = labs((int32_t)ax - baseAx) + labs((int32_t)ay - baseAy) + labs((int32_t)az - baseAz);
  int32_t gyro = labs((int32_t)gx) + labs((int32_t)gy) + labs((int32_t)gz);

  if (dAcc > ACC_THRESH || gyro > GYRO_THRESH) {
    if (++overCount >= TRIGGER_COUNT) {
      setMode(Mode::ALARM);
      lastAlert = millis() - ALERT_REPEAT_MS;  // 즉시 ALERT 전송
    }
  } else {
    overCount = 0;
  }
}

static void runAlarm() {
  uint32_t now = millis();
  uint32_t phase = (now - modeSince) % (BEEP_ON_MS + BEEP_OFF_MS);
  digitalWrite(PIN_BUZZER, phase < BEEP_ON_MS ? HIGH : LOW);

  if (now - lastAlert >= ALERT_REPEAT_MS) {
    lastAlert = now;
    send(F("ALERT:MOVED"));
  }
}

void setup() {
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  bleState.begin();
  pinMode(PIN_MPU_INT, INPUT);

  Serial.begin(SERIAL_BAUD);
  ble.begin(BLE_BAUD);
  Wire.begin();
  mpu.initialize();

  Serial.println(F("# [main] umbrella guard fw=" FW_VERSION " proto=" PROTO_VERSION));
  sensorOk = mpuCheckAndReport();
  if (!sensorOk) {
    // 센서 고장을 알리기 위해 길게 한 번 울림. ARM 명령은 ERR:SENSOR 로 거부된다.
    digitalWrite(PIN_BUZZER, HIGH); delay(1000); digitalWrite(PIN_BUZZER, LOW);
  }
  nextSample = millis();
  setMode(Mode::DISARMED);
}

void loop() {
  if (bleState.update()) onBleConnectionChanged();

  while (Serial.available()) feed(serialCmd, Serial.read());
  while (ble.available()) {
    int ch = ble.read();
    if (bleState.onData()) onBleConnectionChanged();
    feed(bleCmd, ch);
  }
  flushOnTimeout(serialCmd);
  flushOnTimeout(bleCmd);

  uint32_t now = millis();
  if ((int32_t)(now - nextSample) >= 0) {
    nextSample += SAMPLE_PERIOD_MS;
    sampleMotion();
  }

  if (mode == Mode::ALARM) runAlarm();
}
