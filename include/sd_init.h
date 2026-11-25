// microSD 초기화 (재시도 포함).
// 리셋/업로드 직후에는 카드가 이전 상태에 머물러 첫 CMD0 에 응답하지 않는 경우가 흔하다
// (SdError 0x01). 속도를 낮춰 가며 여러 번 재시도한다.
// Uno 는 FAT16/32 전용: SdFat32 <-> File32 짝으로 사용할 것.
#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include "pins.h"

// 성공 시 true. 진행 상황을 시리얼에 출력한다.
inline bool sdBeginRetry(SdFat32& sd, uint8_t maxAttempts = 6) {
  static const uint32_t speeds[] = {4000000UL, 1000000UL, 250000UL};
  delay(100);  // 카드 전원 안정화
  for (uint8_t i = 0; i < maxAttempts; i++) {
    uint32_t hz = speeds[i % 3];
    Serial.print(F("SD init @ "));
    Serial.print(hz / 1000);
    Serial.print(F(" kHz ... "));
    if (sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, hz))) {
      Serial.println(F("OK"));
      return true;
    }
    Serial.print(F("실패 SdError=0x"));
    Serial.print(sd.sdErrorCode(), HEX);
    Serial.print(F(",0x"));
    Serial.println(sd.sdErrorData(), HEX);
    sd.end();
    delay(500);
  }
  return false;
}
