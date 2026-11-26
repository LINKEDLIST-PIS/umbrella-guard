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

| 명령 | 설명 | 응답 |
|---|---|---|
| `ARM` | 보관 모드 시작 | `OK:ARM`, `STATE:SETTLING`, `STATE:ARMED` |
| `DISARM` | 보관 모드와 경보 해제 | `OK:DISARM`, `STATE:DISARMED` |
| `STATUS` | 현재 상태 조회 | `STATE:<상태>` |
| `PING` | 연결 확인 | `PONG` |
| `INFO` | 펌웨어/프로토콜 버전 | `INFO:fw=1.1.0,proto=1,sensor=ok` |

경보 중에는 `ALERT:MOVED`를 2초마다 보냅니다. BLE UUID, 메시지 형식, Android 앱 요구사항은 [docs/APP_SPEC.md](docs/APP_SPEC.md)에 정리되어 있습니다.

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

## 개발 환경 준비

### 펌웨어 빌드와 업로드

1. [VS Code](https://code.visualstudio.com/)와 [Git](https://git-scm.com/)을 설치합니다.
2. VS Code 확장 **PlatformIO IDE**를 설치합니다. 확장이 자체 Python과 PlatformIO Core를 알아서 설치하므로, Python이나 `pip install platformio`는 따로 필요 없습니다.
3. 저장소를 받아 VS Code로 폴더를 엽니다.
   ```bash
   git clone https://github.com/LINKEDLIST-PIS/umbrella-guard.git
   ```
4. 하단 상태바에서 환경(`env:main` 등)을 고르고 빌드(✓) 버튼을 누릅니다. 처음 빌드할 때 AVR 컴파일러, Arduino 프레임워크, 라이브러리를 자동으로 내려받습니다. 몇 분 걸리고 인터넷 연결이 필요합니다.

터미널에서 `pio` 명령을 쓰려면 VS Code의 **PlatformIO Core CLI** 터미널을 엽니다. 일반 터미널에서 쓰려면 `%USERPROFILE%\.platformio\penv\Scripts`(macOS/Linux는 `~/.platformio/penv/bin`)를 PATH에 추가합니다.

> **업로드가 안 될 때**
> - 시리얼 모니터가 열려 있으면 포트가 잡혀 있어서 업로드가 실패합니다. 모니터를 닫고 업로드하세요.
> - 호환 Uno(CH340 USB 칩)는 Windows에서 COM 포트로 잡히지 않을 수 있습니다. 이때는 CH340 드라이버를 설치해야 합니다.

### 데이터 분석 스크립트 (선택)

`tools/analyze_log.py`(로그 분석)나 `tools/make_qr.py`(QR 라벨)를 쓸 때만 필요합니다.

1. [Python](https://www.python.org/) 3.10 이상을 설치합니다. Windows에서는 설치 화면에서 "Add python.exe to PATH"를 체크합니다.
2. 패키지를 설치합니다.
   ```bash
   pip install -r tools/requirements.txt
   ```

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
| `setup_ble` | `src/setup/ble/` | HM-10 1회 설정: 광고 이름 `UMBRELLA`와 PIN 본딩, QR 라벨 정보 출력 |

### HM-10 초기 설정과 QR 라벨 (모듈당 한 번)

PIN은 기기마다 다르고, **QR 라벨 실물로만 관리**합니다. 코드와 저장소에는 PIN이 남지 않습니다.

1. 폰과 연결을 끊은 상태에서 `pio run -e setup_ble -t upload`로 올린 뒤 `pio device monitor`를 엽니다. 모니터 로그를 파일로 저장(`log2file`)하지 마세요.
2. 모드를 고릅니다.
   - `n`: 새 PIN 설정. 직접 정한 6자리 PIN을 입력합니다.
   - `l`: 이미 설정된 모듈의 라벨 정보만 다시 출력합니다.
3. 마지막에 출력되는 `QR: UG1:…` 줄의 값으로 라벨을 만듭니다. 입력 내용은 화면에 표시되지 않습니다.
   ```bash
   pip install -r tools/requirements.txt   # 처음 한 번
   python tools/make_qr.py                 # QR 문자열 붙여넣기 → labels/UMBRELLA-<ID>.png
   ```
4. 라벨을 인쇄해 설명서, 포장, 손잡이 안쪽처럼 주인만 보는 곳에 둡니다. `labels/`는 저장소에 올라가지 않으며, 인쇄 후 파일은 지워도 됩니다.
5. `main`을 다시 업로드합니다. 이전에 이 모듈과 연결했던 폰은 블루투스 설정에서 기기를 삭제한 뒤 다시 등록합니다.

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

[`tools/analyze_log.py`](tools/analyze_log.py)는 SD 카드의 `LOGnn.CSV`와 `pio device monitor` 로그(`d`/`p`로 출력한 내용)를 모두 읽습니다. 필요한 패키지는 [개발 환경 준비](#데이터-분석-스크립트-선택)를 참고하세요.

```bash
python tools/analyze_log.py logs/device-monitor-xxx.log --plot
python tools/analyze_log.py LOG07.CSV --rate 1500 --angle 25  # 기준값을 바꿔 시뮬레이션
```

- 정지 상태의 노이즈를 계산합니다.
- 움직임 구간을 찾아 스파이크, 짧은 흔들림, 긴 흔들림, 자세 변화로 나눕니다.
- `main` 감지기(fw 1.1)의 경보 판정을 펌웨어와 같은 정수 연산으로 재현합니다. 이전 방식(fw 1.0)의 결과도 비교용으로 함께 출력합니다.
- `--plot`을 주면 `logs/plots/`에 그래프를 저장합니다. 그래프에는 가속도, 기울이는 회전, 기울기 각도가 나옵니다.

## 움직임 감지 방식 (fw 1.1)

2026-11-26 실험(LOG07)에서 동작별로 회전하는 방향이 다르다는 것을 확인했습니다.

| 동작 | 우산 길이 방향 축의 비틀림 | 우산을 기울이는 회전 |
|---|---|---|
| 툭 침 | 매우 큼 | 0.3초 안에 사라짐 |
| 들기, 꺼냄 | 작음 | 0.5초 이상 이어짐 |

그래서 **기울이는 회전만** 이동평균해서 판정합니다. 우산 길이 방향 축은 ARM할 때 중력과 가장 가까운 축으로 자동으로 정합니다. [`src/main/main.cpp`](src/main/main.cpp) 맨 위의 값으로 감도를 조정합니다.

| 상수 | 기본값 | 의미 |
|---|---|---|
| `TILT_RATE_THRESH` | 1200 | 기울이는 회전(두 축 각속도 합에서 오프셋 제거)의 지수이동평균(1/32). 이 값을 넘으면 경보 |
| `TILT_ANGLE_DEG` | 20 | 가속도 이동평균 방향이 기준 자세에서 이만큼 기울면 경보. 회전 없이 천천히 들어 올리는 경우 보완 |

LOG07 시뮬레이션 결과, 이전 방식에서는 툭 침 13번이 모두 경보였지만 지금 방식은 1번(세게 친 경우)만 울렸습니다. 들기 동작과 꺼냄은 0~1.1초 안에 모두 감지했습니다. 다만 들기 중 가장 약한 동작(1,353)과 연달아 친 툭 침 중 가장 강한 동작(1,181)이 기준값 1200에 가깝습니다. 실험을 더 해서 확인해야 합니다. 옆 우산을 넣고 빼면서 오래 흔들리는 경우는 두 방식 모두 경보가 울립니다.

시리얼 모니터에 `DEBUG`를 입력하면 ARMED 상태에서 판정값(`rate,angle`)이 초당 10번 출력됩니다. 시리얼 전용 명령이라 BLE로는 동작하지 않습니다.

## 알려진 사항

- **MPU-6050 호환 칩**: WHO_AM_I 값이 `0x68`이 아닌 호환 칩(예: `0x72`)은 라이브러리의 `testConnection()`을 통과하지 못합니다. 그래서 [`include/mpu_check.h`](include/mpu_check.h)에서 I2C 응답 여부로 연결을 판단합니다.
- **microSD 초기화**: 리셋 직후 첫 초기화가 가끔 실패합니다. [`include/sd_init.h`](include/sd_init.h)에서 속도를 낮춰 가며 다시 시도합니다.
- **HM-10 STATE 핀**: 연결되지 않은 상태에서 깜빡이기 때문에, 1.5초 이상 계속 HIGH일 때만 연결로 판단합니다([`include/ble_state.h`](include/ble_state.h)).
- **HM-10 AT 명령**: 한 덩어리로 보내야 응답합니다. `test_ble`은 입력을 한 줄씩 모아서 보냅니다.

## 진행 상황

- [x] 부품별 동작 확인 (부저, MPU-6050, microSD, HM-10)
- [x] BLE 프로토콜, PIN 본딩, 앱 개발 명세 ([docs/APP_SPEC.md](docs/APP_SPEC.md))
- [x] logger로 라벨을 붙인 실험 데이터 1차 수집 (LOG07)
- [x] 감지 방식 1차 개선 (fw 1.1, 시뮬레이션으로 검증)
- [x] fw 1.1 실기기 확인, 기준값 1200 확정 (2026-11-26)
- [x] main 통합 테스트 (BLE, PIN 페어링, 경보)
- [ ] Android 앱 개발 ([docs/APP_SPEC.md](docs/APP_SPEC.md) 기준, 외부 개발)

### fw 1.1 실기기 테스트 결과 (2026-11-26)

| 동작 | 판정값 rate 최대 | 결과 |
|---|---|---|
| 정지 | 약 40 | 경보 없음 |
| 옆 우산 넣고 빼기 | 679 | 경보 없음 |
| 우산꽂이 툭 침 | 732 | 경보 없음 |
| 천천히 살짝 들기 | 1200 넘음 | 약 2.3초 뒤 경보 |
| 보통 속도로 끝까지 꺼내기 | 1200 넘음 | 약 2.1초 뒤 경보 |

기준값을 1000으로 낮추면 꺼낼 때 약 0.5초 만에 경보가 울립니다. 하지만 연달아 세게 치는 경우(LOG07, 최대 1,181)에 오경보가 납니다. 그래서 오경보 방지를 우선해 1200을 유지했습니다. 아주 천천히 수직으로 뽑고 가만히 있으면 IMU만으로는 놓칠 수 있습니다. 이 경우는 꽂힘 감지 센서(홀 센서 등)를 다음 단계에서 검토합니다.
