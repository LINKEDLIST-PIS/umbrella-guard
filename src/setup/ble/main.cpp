// env:setup_ble - HM-10 모듈 1회 설정 (광고 이름 + PIN 본딩)
//
// 설정 내용 (모듈 내부 플래시에 저장되므로 한 번만 실행하면 됨)
//   AT+NAMEUMBRELLA   광고 이름
//   AT+PASS<PIN>      6 자리 PIN  (시리얼 모니터에서 입력, 코드/저장소에 남기지 않음)
//   AT+TYPE3          PIN 인증 + 본딩 (페어링 안 된 폰은 특성 읽기/쓰기 불가)
//   AT+RESET          재시작해야 TYPE 이 적용됨
//
// 사용
//   1) 폰과 연결을 끊는다 (연결 중이면 AT 명령이 폰으로 전달됨)
//   2) pio run -e setup_ble -t upload  ->  pio device monitor
//   3) 안내에 따라 6 자리 PIN 입력 후 Enter
//   4) 끝나면 main 을 다시 업로드
//   * 이전에 이 모듈과 연결했던 폰은 블루투스 설정에서 기기를 '삭제' 후 다시 연결
//
// AT 명령은 줄끝 없이 한 덩어리로 보낸다 (정품 HM-10 은 \r\n 을 이름 등에 포함시킬 수 있음).
#include <Arduino.h>
#include <AltSoftSerial.h>
#include "pins.h"
#include "ble_state.h"

constexpr char DEVICE_NAME[] = "UMBRELLA";  // docs/APP_SPEC.md 와 일치시킬 것 (최대 12자)

AltSoftSerial ble;
BleState state;

char resp[48];

// AT 명령 전송 후 응답을 resp 에 받는다. 응답 길이 반환.
static uint8_t at(const char* cmd, uint16_t waitMs = 600, bool echoCmd = true) {
  while (ble.available()) ble.read();
  if (echoCmd) {
    Serial.print(F("  > "));
    Serial.println(cmd);
  }
  ble.print(cmd);
  uint8_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < waitMs) {
    while (ble.available() && n < sizeof(resp) - 1) {
      char c = ble.read();
      if (c != '\r' && c != '\n') resp[n++] = c;
    }
  }
  resp[n] = '\0';
  Serial.print(F("  < "));
  Serial.println(n ? resp : "(응답 없음)");
  return n;
}

static bool ok() { return strncmp(resp, "OK", 2) == 0; }

static void fail(const __FlashStringHelper* why) {
  Serial.println();
  Serial.print(F("[실패] "));
  Serial.println(why);
  Serial.println(F("응답 내용을 알려주세요. 모듈 펌웨어에 따라 명령 형식이 다를 수 있습니다."));
  while (true) {}
}

static bool readPin(char* pin) {
  uint8_t n = 0;
  while (true) {
    if (!Serial.available()) continue;
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (n == 0) continue;
      pin[n] = '\0';
      return n == 6;
    }
    if (n < 6 && c >= '0' && c <= '9') pin[n++] = c;
    else if (n >= 6 || c < '0' || c > '9') n = 7;  // 잘못된 입력 표시
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  ble.begin(BLE_BAUD);
  state.begin();
  Serial.println(F("[setup_ble] HM-10 설정: 이름 UMBRELLA + PIN 본딩"));

  // 폰이 연결되어 있지 않은지 확인 (STATE 1.6 초 관찰)
  uint32_t t0 = millis();
  while (millis() - t0 < 1600) state.update();
  if (state.connected()) fail(F("폰이 연결되어 있습니다. 앱에서 연결을 끊고 리셋하세요."));

  Serial.println(F("\n1) 모듈 확인"));
  if (!at("AT") || !ok()) fail(F("AT 응답 없음. test_ble 의 ! 진단으로 배선/baud 확인"));
  at("AT+VERS?");
  at("AT+NAME?");
  at("AT+TYPE?");

  char pin[8];
  while (true) {
    Serial.println(F("\n2) 6 자리 PIN 을 입력하고 Enter (예: 482916). 000000 / 123456 은 사용 불가"));
    if (!readPin(pin)) {
      Serial.println(F("   숫자 6 자리만 입력하세요."));
      continue;
    }
    if (strcmp(pin, "000000") == 0 || strcmp(pin, "123456") == 0) {
      Serial.println(F("   기본값/추측 쉬운 PIN 은 사용할 수 없습니다."));
      continue;
    }
    Serial.println(F("   PIN 입력됨 (화면에 표시하지 않음). 이 PIN 을 따로 기록해 두세요."));
    break;
  }

  Serial.println(F("\n3) 설정 적용"));
  char cmd[24];
  snprintf(cmd, sizeof(cmd), "AT+NAME%s", DEVICE_NAME);
  at(cmd);
  if (!ok()) fail(F("이름 설정 실패"));

  snprintf(cmd, sizeof(cmd), "AT+PASS%s", pin);
  Serial.println(F("  > AT+PASS******"));
  at(cmd, 600, false);
  if (!ok()) fail(F("PIN 설정 실패"));

  at("AT+TYPE3");
  if (!ok()) fail(F("인증 방식 설정 실패"));

  at("AT+RESET", 1000);
  delay(1500);  // 모듈 재시작 대기

  Serial.println(F("\n4) 확인"));
  if (!at("AT") || !ok()) fail(F("재시작 후 AT 응답 없음"));
  at("AT+NAME?");
  bool nameOk = strstr(resp, DEVICE_NAME) != nullptr;
  at("AT+TYPE?");
  size_t len = strlen(resp);
  bool typeOk = len > 0 && resp[len - 1] == '3';
  at("AT+PASS?", 600);  // 응답에 PIN 이 보이지만 로컬 시리얼 모니터에만 출력됨
  bool pinOk = strstr(resp, pin) != nullptr;

  Serial.println();
  Serial.print(F("이름 UMBRELLA : ")); Serial.println(nameOk ? F("OK") : F("확인 필요"));
  Serial.print(F("PIN           : ")); Serial.println(pinOk ? F("OK") : F("확인 필요"));
  Serial.print(F("TYPE3 (본딩)  : ")); Serial.println(typeOk ? F("OK") : F("확인 필요"));
  if (nameOk && pinOk && typeOk) {
    Serial.println(F("\n[완료] main 을 다시 업로드하세요."));
    Serial.println(F("이전에 연결했던 폰은 블루투스 설정에서 기기 삭제 후 다시 연결 (PIN 입력 창이 뜸)."));
  } else {
    Serial.println(F("\n일부 확인 실패 - 위 응답을 알려주세요."));
  }
}

void loop() {}
