// 테스트: HM-10 <-> 폰 문자 주고받기
//
// 시리얼 모니터 입력은 한 줄(Enter)씩 모아서 HM-10 으로 한 번에 보낸다.
// (정품 HM-10 은 "AT" 가 한 덩어리로 와야 응답. 한 글자씩 보내면 무시됨)
//
// 특수 명령 (한 줄에 그것만 입력 후 Enter)
//   !  : 자동 진단 - 여러 baud x 줄끝 조합으로 "AT" 를 보내 응답 확인
//   ?  : STATE 핀 원시값 2 초간 출력
//   #  : HM-10 으로 보낼 때 줄끝(\r\n) 붙이기 on/off (기본 off = 정품 HM-10)
//
// 순서
//   1) 폰 미연결 상태에서 "!" -> 응답 오는 조합 확인
//   2) 폰 앱(Android: Serial Bluetooth Terminal, Bluetooth LE 탭)으로 연결
//      -> 1.5 초 뒤 "[STATE] 연결됨", 앱에 "hello from umbrella"
//   3) 양쪽 문자 교환되면 통과
#include <Arduino.h>
#include <AltSoftSerial.h>
#include "pins.h"
#include "ble_state.h"

AltSoftSerial ble;  // RX=D8, TX=D9 고정 (pins.h 참고)
BleState state;

char line[64];
uint8_t lineLen = 0;
bool appendCrLf = false;

static void dumpStatePin() {
  Serial.println(F("STATE 원시값 (50 ms 간격, 2 초):"));
  for (uint8_t i = 0; i < 40; i++) {
    Serial.print(digitalRead(PIN_BLE_STATE));
    delay(50);
  }
  Serial.println(F("\n 0/1 반복=미연결(깜빡임), 전부 1=연결, 전부 0=미연결(PIO11) 또는 미배선"));
}

// 지정 baud / 줄끝으로 AT 전송 후 300 ms 동안 응답 출력. 응답 바이트 수 반환.
static uint8_t probe(uint32_t baud, bool crlf) {
  ble.end();
  ble.begin(baud);
  delay(50);
  while (ble.available()) ble.read();

  Serial.print(F("  "));
  Serial.print(baud);
  Serial.print(crlf ? F(" baud, AT\\r\\n : ") : F(" baud, AT     : "));
  ble.print(crlf ? F("AT\r\n") : F("AT"));

  uint8_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < 300) {
    while (ble.available()) {
      char c = ble.read();
      n++;
      if (c >= 32 && c < 127) Serial.write(c);
      else { Serial.print(F("<")); Serial.print((uint8_t)c, HEX); Serial.print(F(">")); }
    }
  }
  Serial.println(n ? F("  <-- 응답!") : F("(없음)"));
  return n;
}

static void autoProbe() {
  if (state.connected()) {
    Serial.println(F("폰이 연결된 상태에서는 AT 가 폰으로 전달됨. 연결을 끊고 다시 시도"));
    return;
  }
  Serial.println(F("자동 진단 시작 (폰 연결 끊긴 상태여야 함)"));
  static const uint32_t bauds[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400};
  bool any = false;
  for (uint32_t b : bauds) {
    if (probe(b, false)) any = true;
    if (probe(b, true)) any = true;
  }
  ble.end();
  ble.begin(BLE_BAUD);
  if (any) {
    Serial.println(F("응답이 온 조합을 알려주세요. (9600 이 아니면 pins.h 의 BLE_BAUD 를 맞추거나 모듈 설정 변경)"));
  } else {
    Serial.println(F("모든 조합 무응답 -> 배선 확인:"));
    Serial.println(F("  HM-10 TXD -> D8, D9 -> 분압 -> HM-10 RXD (TX/RX 교차 확인)"));
    Serial.println(F("  HM-10 VCC/GND, 분압 저항값(예: 1k/2k) 확인"));
  }
}

static void handleLine() {
  line[lineLen] = '\0';
  if (strcmp(line, "!") == 0) autoProbe();
  else if (strcmp(line, "?") == 0) dumpStatePin();
  else if (strcmp(line, "#") == 0) {
    appendCrLf = !appendCrLf;
    Serial.println(appendCrLf ? F("줄끝 \\r\\n 붙임 (호환 모듈용)") : F("줄끝 없음 (정품 HM-10용)"));
  } else if (lineLen) {
    ble.print(line);
    if (appendCrLf) ble.print(F("\r\n"));
    Serial.print(F("> "));
    Serial.println(line);
  }
  lineLen = 0;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  ble.begin(BLE_BAUD);
  state.begin();
  Serial.println(F("[test_ble] 한 줄씩 HM-10 으로 전송. 명령: ! 자동진단, ? STATE, # 줄끝 토글"));
  Serial.println(F("[STATE] 연결 안 됨"));
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (lineLen) handleLine();
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = c;
    }
  }
  while (ble.available()) Serial.write(ble.read());

  if (state.update()) {
    Serial.println(state.connected() ? F("\n[STATE] 연결됨") : F("\n[STATE] 연결 안 됨"));
    if (state.connected()) ble.print(F("hello from umbrella\r\n"));
  }
}
