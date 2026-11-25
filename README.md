# Umbrella Guard

우산꽂이에 꽂아 둔 우산의 도난을 감지하는 시제품입니다.
보관 모드에서 우산이 움직이면 **부저를 울리고 BLE로 스마트폰에 알림**을 보냅니다.

- 보드: Arduino Uno R3 (ATmega328P)
- 개발 환경: [PlatformIO](https://platformio.org/)

## 동작

```
DISARMED ──ARM──▶ SETTLING (3 s) ──▶ ARMED ──움직임 감지──▶ ALARM
    ▲                                                       │
    └─────────────────────── DISARM ────────────────────────┘
```

- **ARM**: 3초 동안 우산을 내려놓을 시간을 준 뒤, 그때의 자세를 기준으로 저장합니다.
- **ARMED**: 가속도가 기준에서 벗어나거나 회전이 감지되는 샘플이 연속으로 나오면 경보 상태로 넘어갑니다.
- **ALARM**: 부저가 울리고, BLE로 `ALERT:MOVED`를 2초마다 보냅니다. `DISARM`을 보내면 해제됩니다.

명령은 BLE와 USB 시리얼 양쪽에서 받습니다. 대소문자는 구분하지 않습니다.

| 명령 | 설명 |
|---|---|
| `ARM` | 보관 모드 시작 |
| `DISARM` | 보관 모드와 경보 해제 |
| `STATUS` | 현재 상태 응답 |

## 하드웨어

| 부품 | 용도 |
|---|---|
| MPU-6050 | 6축 가속도/자이로 센서 (I2C) |
| HM-10 | BLE 4.0 모듈 (UART, 3.3 V 로직) |
| 능동 부저 + NPN 트랜지스터 + 플라이백 다이오드 | 경보음 |
| microSD 모듈 | 실험 데이터 기록용 (`logger` 환경에서만 사용) |

### 배선

| 부품 | 신호 | Uno 핀 |
|---|---|---|
| MPU-6050 | SDA / SCL | A4 / A5 |
| | INT | D2 |
| HM-10 | TXD → | D8 |
| | RXD ← (분압 회로 경유) | D9 |
| | STATE | D7 |
| microSD | CS / MOSI / MISO / SCK | D10 / D11 / D12 / D13 |
| 부저 | 트랜지스터 베이스 | D6 (HIGH = ON) |

핀 번호는 [`include/pins.h`](include/pins.h)에서만 정의합니다.

> **배선 주의**
> - HM-10 RXD는 3.3 V 입력입니다. D9의 5 V 신호는 분압 회로를 거쳐 연결합니다(예: 1 kΩ / 2 kΩ).
> - 부저의 플라이백 다이오드는 띠(캐소드)가 +5 V 쪽을 향하게 꽂습니다. 반대로 꽂으면 부저가 단락되어 소리가 나지 않습니다.

## 빌드 환경

`platformio.ini`에서 `build_src_filter`로 소스를 나눕니다.

| 환경 | 소스 | 용도 |
|---|---|---|
| `main` | `src/main/` | 최종 기능 (MPU-6050 + HM-10 + 부저) |
| `logger` | `src/logger/` | MPU-6050 데이터를 microSD에 CSV로 기록 (감지 임계값 조정용) |
| `test_mpu` | `src/tests/mpu/` | MPU-6050 값을 시리얼로 출력 |
| `test_buzzer` | `src/tests/buzzer/` | 부저 패턴 출력, tone 출력, 계속 HIGH 출력 진단 |
| `test_ble` | `src/tests/ble/` | 시리얼 모니터와 HM-10 사이 중계, AT 자동 진단 |
| `test_sd` | `src/tests/sd/` | microSD 쓰기/읽기 |

```bash
pio run                          # main + logger 빌드
pio run -e test_mpu -t upload    # 특정 환경을 빌드하고 업로드
pio device monitor               # 시리얼 모니터 (115200 baud)
```

사용하는 라이브러리는 `electroniccats/MPU6050`, `paulstoffregen/AltSoftSerial`, `greiman/SdFat`입니다. PlatformIO가 빌드할 때 자동으로 설치합니다.

## logger 사용법

부팅하면 `LOG00.CSV`부터 비어 있는 번호로 새 파일을 만들고, 50 Hz로 `ms,ax,ay,az,gx,gy,gz,mark`를 기록합니다.

| 키 | 동작 |
|---|---|
| `0`~`9` | mark 값 변경 (실험 구간 표시. 예: 0=가만히, 1=꺼냄, 2=부딪힘, 3=진동) |
| `v` | 실시간 보기 켜기/끄기 (10 Hz) |
| `s` | 기록 종료 |
| `l` | 파일 목록 |
| `d` / `p` | 현재 파일 / 직전 파일 내용 출력 |
| `n` | 새 파일로 기록 재시작 |

시리얼 모니터를 열면 Uno가 리셋되면서 새 파일이 생깁니다. 직전 실험 데이터는 `p`로 확인하세요.

### 데이터 분석

[`tools/analyze_log.py`](tools/analyze_log.py)는 SD 카드의 `LOGnn.CSV`와 `pio device monitor` 로그(`d`/`p`로 출력한 내용)를 모두 읽습니다. Python 3과 numpy가 필요하고, `--plot`을 쓰려면 matplotlib도 필요합니다.

```bash
python tools/analyze_log.py logs/device-monitor-xxx.log --plot
python tools/analyze_log.py LOG07.CSV --gyro 1000 --count 3   # 임계값을 바꿔 시뮬레이션
```

- 정지 상태의 노이즈를 계산합니다.
- 움직임 구간을 찾아 스파이크, 짧은 흔들림, 긴 흔들림, 자세 변화로 나눕니다.
- `main` 감지기의 경보 판정을 재현합니다.
- `--plot`을 주면 `logs/plots/`에 그래프를 저장합니다.

## 감지 임계값 조정

[`src/main/main.cpp`](src/main/main.cpp) 맨 위의 값으로 감도를 조정합니다.

| 상수 | 기본값 | 의미 |
|---|---|---|
| `ACC_THRESH` | 2500 | 기준 자세 대비 가속도 변화 합 (약 0.15 g) |
| `GYRO_THRESH` | 2600 | 각속도 합 (약 20 dps) |
| `TRIGGER_COUNT` | 3 | 연속으로 초과해야 하는 샘플 수 |

기본값은 초기 추정치입니다. `logger`로 실제 사용 환경의 데이터를 모은 뒤 조정할 예정입니다.

## 알려진 사항

- **MPU-6050 호환 칩**: WHO_AM_I 값이 `0x68`이 아닌 호환 칩(예: `0x72`)은 라이브러리의 `testConnection()`을 통과하지 못합니다. 그래서 [`include/mpu_check.h`](include/mpu_check.h)에서 I2C 응답 여부로 연결을 판단합니다.
- **microSD 초기화**: 리셋 직후 첫 초기화가 가끔 실패합니다. [`include/sd_init.h`](include/sd_init.h)에서 속도를 낮춰 가며 다시 시도합니다.
- **HM-10 STATE 핀**: 연결되지 않은 상태에서 깜빡이기 때문에, 1.5초 이상 계속 HIGH일 때만 연결로 판단합니다([`include/ble_state.h`](include/ble_state.h)).
- **HM-10 AT 명령**: 한 덩어리로 보내야 응답합니다. `test_ble`은 입력을 한 줄씩 모아서 보냅니다.

## 진행 상황

- [x] 부품별 동작 확인 (부저, MPU-6050, microSD, HM-10)
- [ ] logger로 실사용 데이터 수집
- [ ] 감지 임계값 조정
- [ ] main 통합 테스트
