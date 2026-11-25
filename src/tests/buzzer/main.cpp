// 테스트: 부저 울리기 (D6 HIGH = ON, NPN 트랜지스터 구동)
//
// 시리얼 명령
//   'p' : 패턴 모드 (기본) - 짧게 3번 -> 길게 1번 -> 2초 쉬기 반복 (능동 부저용)
//   'h' : D6 를 계속 HIGH 로 유지 - 멀티미터로 전압 측정용
//   't' : D6 에 2 kHz 구형파 출력 (tone) - 수동 부저면 이때만 소리가 남
//   '0' : 정지 (D6 LOW)
//
// 진단:
//   'p' 에선 무음인데 't' 에서 소리 남  -> 수동 부저. main 은 tone 방식으로 바꿔야 함
//   'h' 에서 D6-GND 가 약 5 V 인데 무음 -> 트랜지스터/부저 결선 문제
//   'h' 에서 D6-GND 가 0 V             -> 보드 D6 핀 문제
#include <Arduino.h>
#include "pins.h"

enum class Mode : char { PATTERN = 'p', HOLD = 'h', TONE = 't', OFF = '0' };
Mode mode = Mode::PATTERN;
uint32_t since = 0;

static void setMode(Mode m) {
  mode = m;
  since = millis();
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  switch (m) {
    case Mode::PATTERN: Serial.println(F("패턴 모드 (능동 부저 테스트)")); break;
    case Mode::HOLD:
      digitalWrite(PIN_BUZZER, HIGH);
      Serial.println(F("D6 계속 HIGH - D6-GND 전압, 트랜지스터 C-E 전압 측정"));
      break;
    case Mode::TONE:
      tone(PIN_BUZZER, 2000);
      Serial.println(F("2 kHz tone (수동 부저 테스트)"));
      break;
    case Mode::OFF: Serial.println(F("정지")); break;
  }
}

void setup() {
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  Serial.println(F("[test_buzzer] p=패턴, h=계속HIGH, t=2kHz tone, 0=정지"));
  setMode(Mode::PATTERN);
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'p' || c == 'h' || c == 't' || c == '0') setMode(static_cast<Mode>(c));
  }

  if (mode == Mode::PATTERN) {
    // 0~600 ms: 짧게 3번(100 on/100 off), 600~1200 ms: 길게, 1200~3200 ms: 쉼
    uint32_t t = (millis() - since) % 3200;
    bool on = (t < 600) ? ((t / 100) % 2 == 0) : (t < 1200);
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
  }
}
