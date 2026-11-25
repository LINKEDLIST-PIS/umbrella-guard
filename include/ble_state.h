// HM-10 STATE 핀 해석.
// 기본 설정(AT+PIO10)에서는 미연결 시 STATE 가 500 ms 간격으로 깜빡이고,
// 연결 시 계속 HIGH 가 된다. 그래서 일정 시간 이상 HIGH 가 유지될 때만 '연결'로 본다.
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
    bool high = digitalRead(PIN_BLE_STATE) == HIGH;
    if (!high) highSince_ = now;
    bool c = high && (now - highSince_ >= CONNECT_HOLD_MS);
    if (c != connected_) {
      connected_ = c;
      return true;
    }
    return false;
  }

  bool connected() const { return connected_; }

 private:
  uint32_t highSince_ = 0;
  bool connected_ = false;
};
