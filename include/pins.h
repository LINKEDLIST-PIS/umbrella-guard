// 핀 배정은 이 파일 한 곳에서만 정의한다. 다른 파일에 핀 번호를 직접 쓰지 말 것.
// 하드웨어 배선이 고정되어 있으므로 값을 바꾸지 말 것.
#pragma once
#include <Arduino.h>

// ---- MPU-6050 (I2C) ----
// SDA=A4, SCL=A5 는 Uno 하드웨어 I2C 고정 핀 (Wire 라이브러리가 사용)
constexpr uint8_t PIN_MPU_SDA  = A4;
constexpr uint8_t PIN_MPU_SCL  = A5;
constexpr uint8_t PIN_MPU_INT  = 2;     // D2 (INT0)
constexpr uint8_t MPU_I2C_ADDR = 0x68;  // AD0 = LOW

// ---- HM-10 BLE (AltSoftSerial) ----
// AltSoftSerial 은 Uno 에서 RX=D8, TX=D9 로 고정 (Timer1 사용, D10 PWM 불가)
constexpr uint8_t PIN_BLE_RX    = 8;    // D8 <- HM-10 TXD
constexpr uint8_t PIN_BLE_TX    = 9;    // D9 -> 분압 -> HM-10 RXD
constexpr uint8_t PIN_BLE_STATE = 7;    // D7 <- HM-10 STATE (연결 시 HIGH)
constexpr uint32_t BLE_BAUD     = 9600; // HM-10 공장 기본값

// ---- microSD (하드웨어 SPI) ----
constexpr uint8_t PIN_SD_CS   = 10;
constexpr uint8_t PIN_SD_MOSI = 11;
constexpr uint8_t PIN_SD_MISO = 12;
constexpr uint8_t PIN_SD_SCK  = 13;

// ---- 부저 (NPN 트랜지스터 구동, HIGH = ON) ----
constexpr uint8_t PIN_BUZZER = 6;

// ---- 시리얼 ----
constexpr uint32_t SERIAL_BAUD = 115200; // platformio.ini 의 monitor_speed 와 동일
