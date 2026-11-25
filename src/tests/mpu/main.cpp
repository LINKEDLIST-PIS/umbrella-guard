// 테스트: MPU-6050 원시값을 시리얼로 출력 (10 Hz)
#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>
#include "pins.h"
#include "mpu_check.h"

MPU6050 mpu(MPU_I2C_ADDR);

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(PIN_MPU_INT, INPUT);
  Wire.begin();

  Serial.println(F("[test_mpu] MPU-6050 init..."));
  mpu.initialize();
  mpuCheckAndReport();
  Serial.println(F("ax\tay\taz\tgx\tgy\tgz\tINT"));
}

void loop() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  Serial.print(ax); Serial.print('\t');
  Serial.print(ay); Serial.print('\t');
  Serial.print(az); Serial.print('\t');
  Serial.print(gx); Serial.print('\t');
  Serial.print(gy); Serial.print('\t');
  Serial.print(gz); Serial.print('\t');
  Serial.println(digitalRead(PIN_MPU_INT));
  delay(100);
}
