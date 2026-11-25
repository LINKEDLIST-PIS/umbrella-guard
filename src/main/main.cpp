// env:main - 우산 도난 감지 최종 기능 (MPU-6050 + HM-10 + 부저, SD 없음)
//
// 상태
//   DISARMED : 대기. 움직여도 반응 없음
//   ARMED    : 보관 모드. 기준 자세를 잡은 뒤 움직임이 감지되면 ALARM
//   ALARM    : 부저 울림 + BLE 로 "ALERT" 주기 전송. DISARM 명령으로 해제
//
// 명령 (BLE 또는 USB 시리얼, 대소문자 무관, 줄바꿈 없어도 100 ms 후 처리)
//   ARM / DISARM / STATUS
//
// 임계값은 env:logger 로 수집한 데이터를 보고 조정할 것.
#include <Arduino.h>
#include <Wire.h>
#include <AltSoftSerial.h>
#include <MPU6050.h>
#include "pins.h"
#include "mpu_check.h"
#include "ble_state.h"

// ---- 튜닝 값 ----
constexpr uint16_t SAMPLE_PERIOD_MS  = 20;    // 50 Hz
constexpr uint16_t SETTLE_MS         = 3000;  // ARM 후 내려놓을 시간
constexpr int32_t  ACC_THRESH        = 2500;  // |a - 기준| 합, 약 0.15 g (16384 LSB/g)
constexpr int32_t  GYRO_THRESH       = 2600;  // |g| 합, 약 20 dps (131 LSB/dps)
constexpr uint8_t  TRIGGER_COUNT     = 3;     // 연속 N 샘플 초과 시 경보 (노이즈 억제)
constexpr uint16_t ALERT_REPEAT_MS   = 2000;  // ALARM 중 BLE 재전송 주기
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
uint8_t overCount = 0;
bool bleConnected = false;

// ---- 출력 (시리얼 + BLE 동시) ----
static void report(const __FlashStringHelper* msg) {
  Serial.println(msg);
  if (bleConnected) {
    ble.print(msg);
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

static void setMode(Mode m) {
  mode = m;
  modeSince = millis();
  overCount = 0;
  if (m != Mode::ALARM) digitalWrite(PIN_BUZZER, LOW);
  report(modeName());
}

// ---- 명령 처리 ----
struct CmdBuf {
  char buf[12];
  uint8_t len = 0;
  uint32_t lastRx = 0;
};
CmdBuf serialCmd, bleCmd;

static void handleCommand(char* s) {
  for (char* p = s; *p; p++) *p = toupper(*p);
  if (strcmp(s, "ARM") == 0) {
    if (mode == Mode::DISARMED) setMode(Mode::SETTLING);
    else report(modeName());
  } else if (strcmp(s, "DISARM") == 0) {
    setMode(Mode::DISARMED);
  } else if (strcmp(s, "STATUS") == 0) {
    report(modeName());
  } else if (s[0]) {
    report(F("ERR? ARM|DISARM|STATUS"));
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

// ---- 동작 감지 ----
static void sampleMotion() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  if (mode == Mode::SETTLING) {
    if (millis() - modeSince >= SETTLE_MS) {
      baseAx = ax; baseAy = ay; baseAz = az;
      setMode(Mode::ARMED);
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
    report(F("ALERT:MOVED"));
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

  Serial.println(F("[main] umbrella guard"));
  if (!mpuCheckAndReport()) {
    // 센서 고장을 알리기 위해 길게 한 번 울림
    digitalWrite(PIN_BUZZER, HIGH); delay(1000); digitalWrite(PIN_BUZZER, LOW);
  }
  nextSample = millis();
  setMode(Mode::DISARMED);
}

void loop() {
  // BLE 연결 상태 (STATE 핀)
  if (bleState.update()) {
    bleConnected = bleState.connected();
    Serial.println(bleConnected ? F("BLE 연결") : F("BLE 해제"));
    if (bleConnected) report(modeName());
  }

  while (Serial.available()) feed(serialCmd, Serial.read());
  while (ble.available()) feed(bleCmd, ble.read());
  flushOnTimeout(serialCmd);
  flushOnTimeout(bleCmd);

  uint32_t now = millis();
  if ((int32_t)(now - nextSample) >= 0) {
    nextSample += SAMPLE_PERIOD_MS;
    sampleMotion();
  }

  if (mode == Mode::ALARM) runAlarm();
}
