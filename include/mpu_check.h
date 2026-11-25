// MPU-6050 연결 확인.
// 라이브러리의 testConnection() 은 정품 ID(0x68) 계열만 허용해서
// 시중의 호환(클론) 칩(WHO_AM_I = 0x70, 0x72 등)을 실패로 판정한다.
// 여기서는 WHO_AM_I 레지스터를 직접 읽어 I2C 응답이 있으면 성공으로 본다.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"

constexpr uint8_t MPU_REG_WHO_AM_I = 0x75;

// 성공 시 true, whoAmI 에 레지스터 원값 저장. I2C 무응답/버스 이상이면 false.
inline bool mpuCheck(uint8_t& whoAmI) {
  whoAmI = 0;
  Wire.beginTransmission(MPU_I2C_ADDR);
  Wire.write(MPU_REG_WHO_AM_I);
  if (Wire.endTransmission(false) != 0) return false;  // 주소 0x68 에서 ACK 없음
  if (Wire.requestFrom(MPU_I2C_ADDR, (uint8_t)1) != 1) return false;
  whoAmI = Wire.read();
  return whoAmI != 0x00 && whoAmI != 0xFF;
}

// 결과를 시리얼에 출력하고 성공 여부를 돌려준다.
inline bool mpuCheckAndReport() {
  uint8_t id;
  bool ok = mpuCheck(id);
  Serial.print(F("MPU WHO_AM_I = 0x"));
  Serial.print(id, HEX);
  if (!ok) {
    Serial.println(F(" -> 연결 실패: 배선(A4/A5, VCC, GND)과 주소 0x68 확인"));
  } else if (id == 0x68) {
    Serial.println(F(" -> 정품 MPU-6050"));
  } else {
    Serial.println(F(" -> 호환 칩 (응답 정상, 사용 가능)"));
  }
  return ok;
}
