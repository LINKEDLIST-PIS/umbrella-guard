// HM-10 STATE 핀 해석.
// 기본 설정(AT+PIO10)에서는 미연결 시 STATE 가 500 ms 간격으로 깜빡이고,
// 연결 시 계속 HIGH 가 된다.
//
// 판정
//   연결  : STATE 가 CONNECT_HOLD_MS 이상 계속 HIGH  또는  BLE 로 데이터 수신(onData)
//   해제  : STATE 가 LOW 가 되는 즉시
// (AT+PIO11 로 바꾸면 미연결 LOW / 연결 HIGH 가 되지만, 이 판정은 두 설정 모두에서 동작)
#pragma once
#include <Arduino.h>
#include "pins.h"

class BleState {
 public:
  static constexpr uint16_t CONNECT_HOLD_MS = 1500;  // 깜빡임 주기(1 s)보다 길게

  void begin() {
    pinMode(PIN_BLE_STATE, INPUT);
    highSince_ = millis();
  }

  // 매 loop 호출. 연결 상태가 바뀐 순간 true 를 돌려준다.
  bool update() {
    uint32_t now = millis();
    bool c;
    if (digitalRead(PIN_BLE_STATE) == LOW) {
      highSince_ = now;
      c = false;
    } else {
      c = connected_ || (now - highSince_ >= CONNECT_HOLD_MS);
    }
    return set(c);
  }

  // BLE 로 바이트를 받았을 때 호출. 폰이 데이터를 보냈다면 연결된 것이 확실하므로
  // 1.5 초를 기다리지 않고 바로 연결로 본다. 상태가 바뀌었으면 true.
  bool onData() { return set(true); }

  bool connected() const { return connected_; }

 private:
  bool set(bool c) {
    if (c == connected_) return false;
    connected_ = c;
    return true;
  }

  uint32_t highSince_ = 0;
  bool connected_ = false;
};
