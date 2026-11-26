// env:main - 우산 도난 감지 최종 기능 (MPU-6050 + HM-10 + 부저, SD 없음)
//
// 상태
//   DISARMED : 대기. 움직여도 반응 없음
//   SETTLING : ARM 직후 내려놓을 시간 (SETTLE_MS). 마지막 0.5 초 평균으로 기준 자세/자이로 오프셋 저장
//   ARMED    : 보관 모드. 아래 감지 조건을 만족하면 ALARM
//   ALARM    : 부저 울림 + "ALERT:MOVED" 주기 전송. DISARM 명령으로 해제
//
// 감지 방식 (fw 1.1.0, logs/device-monitor-261126-095043.log 의 LOG07 실험 분석 결과)
//   우산 길이 방향 축 = ARM 시 중력과 가장 가까운 축. 나머지 두 축의 회전이 '기울이는 회전'.
//   - 툭 침   : 길이 방향 축 비틀림이 크고, 기울이는 회전은 0.3 초 안에 사라짐
//   - 들기/꺼냄: 기울이는 회전이 0.5 초 이상 이어짐
//   경보 조건 (둘 중 하나)
//   1) 기울이는 회전(|g1|+|g2|, 오프셋 제거)의 지수이동평균(1/32) > TILT_RATE_THRESH
//   2) 가속도 지수이동평균(1/16) 방향이 기준 자세에서 TILT_ANGLE_DEG 이상 기울어짐
//      (회전 없이 천천히 들어 올리는 경우 보완)
//
// BLE / 시리얼 프로토콜은 docs/APP_SPEC.md 참고 (PROTO_VERSION 과 함께 관리).
//   앱 -> 기기 : ARM, DISARM, STATUS, PING, INFO   (줄바꿈 또는 100 ms 무입력으로 종료)
//   기기 -> 앱 : STATE:<모드>, OK:<명령>, ALERT:MOVED, PONG, INFO:..., ERR:<코드>  (\r\n 종료)
//   시리얼 전용: DEBUG  - 판정값을 10 Hz 로 출력 (튜닝용, BLE 로는 동작하지 않음)
//
// 임계값은 env:logger 로 수집한 데이터와 tools/analyze_log.py 로 조정할 것.
#include <Arduino.h>
#include <Wire.h>
#include <AltSoftSerial.h>
#include <MPU6050.h>
#include <math.h>
#include "pins.h"
#include "mpu_check.h"
#include "ble_state.h"

// ---- 버전 (프로토콜을 바꾸면 PROTO_VERSION 을 올리고 docs/APP_SPEC.md 갱신) ----
#define FW_VERSION    "1.1.0"
#define PROTO_VERSION "1"

// ---- 튜닝 값 (tools/analyze_log.py 의 --rate / --angle 과 같은 의미) ----
constexpr uint16_t SAMPLE_PERIOD_MS  = 20;    // 50 Hz
constexpr uint16_t SETTLE_MS         = 3000;  // ARM 후 내려놓을 시간
constexpr uint8_t  BASELINE_SAMPLES  = 25;    // SETTLING 마지막 0.5 초 평균을 기준으로
constexpr int32_t  TILT_RATE_THRESH  = 1200;  // 기울이는 회전 EMA (LSB, 131 LSB = 1 dps)
constexpr uint8_t  TILT_RATE_SHIFT   = 5;     // EMA 계수 1/32 (시정수 약 0.64 s)
constexpr float    TILT_ANGLE_DEG    = 20.0f; // 기준 자세 대비 기울기 경보 각도
constexpr uint8_t  ACC_EMA_SHIFT     = 4;     // 가속도 EMA 계수 1/16 (시정수 약 0.32 s)
constexpr uint16_t ALERT_REPEAT_MS   = 2000;  // ALARM 중 ALERT 재전송 주기
constexpr uint16_t BEEP_ON_MS        = 200;
constexpr uint16_t BEEP_OFF_MS       = 200;
constexpr uint16_t CMD_TIMEOUT_MS    = 100;
constexpr uint8_t  DEBUG_EVERY       = 5;     // DEBUG 출력: 5 샘플마다 (10 Hz)

enum class Mode : uint8_t { DISARMED, SETTLING, ARMED, ALARM };

MPU6050 mpu(MPU_I2C_ADDR);
AltSoftSerial ble;  // RX=D8, TX=D9 고정
BleState bleState;

Mode mode = Mode::DISARMED;
uint32_t modeSince = 0;
uint32_t nextSample = 0;
uint32_t lastAlert = 0;
bool sensorOk = false;
bool debugOut = false;
uint8_t debugCount = 0;

// 기준 (SETTLING 끝에 확정)
int32_t sumA[3], sumG[3];
uint8_t baseCount = 0;
int16_t baseA[3];      // 기준 가속도
int16_t gyroOff[3];    // 자이로 오프셋
uint8_t axisAlong = 0; // 우산 길이 방향 축 (0=X, 1=Y, 2=Z)
float baseNorm = 1.0f;
float tiltCos = 0.94f; // cos(TILT_ANGLE_DEG), setup 에서 계산

// 판정 상태 (고정소수점 EMA: 실제 값 = Q >> SHIFT)
int32_t rateQ = 0;
int32_t accQ[3];

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
  if (m == Mode::SETTLING) {
    baseCount = 0;
    for (uint8_t i = 0; i < 3; i++) sumA[i] = sumG[i] = 0;
  }
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

static void handleCommand(char* s, bool fromSerial) {
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
  } else if (fromSerial && strcmp(s, "DEBUG") == 0) {
    debugOut = !debugOut;
    if (debugOut) {
      Serial.print(F("# DEBUG ON (ARMED 에서 출력): rate,angle_deg  경보 기준 rate>"));
      Serial.print(TILT_RATE_THRESH);
      Serial.print(F(" 또는 angle>"));
      Serial.println(TILT_ANGLE_DEG, 0);
    } else {
      Serial.println(F("# DEBUG OFF"));
    }
  } else {
    send(F("ERR:UNKNOWN"));
  }
}

static void feed(CmdBuf& c, int ch, bool fromSerial) {
  c.lastRx = millis();
  if (ch == '\r' || ch == '\n') {
    c.buf[c.len] = '\0';
    handleCommand(c.buf, fromSerial);
    c.len = 0;
  } else if (c.len < sizeof(c.buf) - 1) {
    c.buf[c.len++] = (char)ch;
  }
}

static void flushOnTimeout(CmdBuf& c, bool fromSerial) {
  if (c.len && millis() - c.lastRx >= CMD_TIMEOUT_MS) feed(c, '\n', fromSerial);
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
static void finishBaseline() {
  int16_t maxAbs = -1;
  for (uint8_t i = 0; i < 3; i++) {
    baseA[i] = sumA[i] / baseCount;
    gyroOff[i] = sumG[i] / baseCount;
    accQ[i] = (int32_t)baseA[i] << ACC_EMA_SHIFT;
    int16_t a = abs(baseA[i]);
    if (a > maxAbs) {
      maxAbs = a;
      axisAlong = i;
    }
  }
  baseNorm = sqrtf((float)baseA[0] * baseA[0] + (float)baseA[1] * baseA[1] + (float)baseA[2] * baseA[2]);
  if (baseNorm < 1.0f) baseNorm = 1.0f;
  rateQ = 0;
}

// 현재 가속도 EMA 방향과 기준 자세 사이 각도의 cos
static float tiltCosNow() {
  float l[3], dot = 0, n2 = 0;
  for (uint8_t i = 0; i < 3; i++) {
    l[i] = (float)(accQ[i] >> ACC_EMA_SHIFT);
    dot += l[i] * baseA[i];
    n2 += l[i] * l[i];
  }
  if (n2 < 1.0f) return 1.0f;
  return dot / (sqrtf(n2) * baseNorm);
}

static void sampleMotion() {
  int16_t a[3], g[3];
  mpu.getMotion6(&a[0], &a[1], &a[2], &g[0], &g[1], &g[2]);

  if (mode == Mode::SETTLING) {
    uint32_t elapsed = millis() - modeSince;
    if (elapsed + (uint32_t)BASELINE_SAMPLES * SAMPLE_PERIOD_MS >= SETTLE_MS) {
      for (uint8_t i = 0; i < 3; i++) {
        sumA[i] += a[i];
        sumG[i] += g[i];
      }
      if (++baseCount >= BASELINE_SAMPLES) {
        finishBaseline();
        setMode(Mode::ARMED);
      }
    }
    return;
  }
  if (mode != Mode::ARMED) return;

  // 1) 기울이는 회전: 길이 방향 축을 뺀 나머지 두 축
  int32_t rate = 0;
  for (uint8_t i = 0; i < 3; i++) {
    if (i == axisAlong) continue;
    rate += labs((int32_t)g[i] - gyroOff[i]);
  }
  rateQ += rate - (rateQ >> TILT_RATE_SHIFT);
  int32_t rateEma = rateQ >> TILT_RATE_SHIFT;

  // 2) 자세 기울기
  for (uint8_t i = 0; i < 3; i++) accQ[i] += a[i] - (accQ[i] >> ACC_EMA_SHIFT);
  float c = tiltCosNow();

  if (debugOut && ++debugCount >= DEBUG_EVERY) {
    debugCount = 0;
    Serial.print(F("# "));
    Serial.print(rateEma);
    Serial.print(',');
    Serial.println(degrees(acosf(constrain(c, -1.0f, 1.0f))), 1);
  }

  if (rateEma > TILT_RATE_THRESH || c < tiltCos) {
    setMode(Mode::ALARM);
    lastAlert = millis() - ALERT_REPEAT_MS;  // 즉시 ALERT 전송
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
  tiltCos = cosf(radians(TILT_ANGLE_DEG));

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

  while (Serial.available()) feed(serialCmd, Serial.read(), true);
  while (ble.available()) {
    int ch = ble.read();
    if (bleState.onData()) onBleConnectionChanged();
    feed(bleCmd, ch, false);
  }
  flushOnTimeout(serialCmd, true);
  flushOnTimeout(bleCmd, false);

  uint32_t now = millis();
  if ((int32_t)(now - nextSample) >= 0) {
    nextSample += SAMPLE_PERIOD_MS;
    sampleMotion();
  }

  if (mode == Mode::ALARM) runAlarm();
}
