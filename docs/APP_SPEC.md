# Umbrella Guard Android 앱 개발 명세

| 항목 | 값 |
|---|---|
| 대상 | Android 앱 개발자 (Kotlin, 네이티브) |
| 펌웨어 | `fw=1.1.0` (`src/main/main.cpp`) |
| 프로토콜 | `proto=1` |
| 최소 Android | 8.0 (API 26) 권장, Android 12+ 권한 모델 대응 필수 |

이 문서는 기기(우산 손잡이 속 Arduino + HM-10)와 앱 사이의 **통신 규약**과 **앱이 갖춰야 할 동작**을 정의합니다. 앱의 화면 디자인과 내부 구조는 개발자가 정하면 됩니다. 다만 5장의 BLE 처리 지침은 이 하드웨어의 특성 때문에 지켜야 하는 내용입니다.

---

## 1. 시스템 개요

```
┌────────────── 우산 (기기) ──────────────┐          ┌──── Android 폰 ────┐
│ MPU-6050 ─ Arduino Uno ─UART─ HM-10    │ ~~BLE~~  │  Umbrella Guard 앱 │
│ (움직임)    (판단/부저)        (BLE 모듈) │          │                    │
└─────────────────────────────────────────┘          └────────────────────┘
```

- 기기는 **보관 모드**(ARMED)일 때 우산이 움직이면 부저를 울리고 앱에 `ALERT:MOVED`를 보냅니다.
- 앱은 기기를 **무장**(ARM)하거나 **해제**(DISARM)하고, 경보를 **시스템 알림**으로 보여 줍니다.
- 앱은 **연결이 끊기는 것**도 감지해 경고합니다. 끊김은 우산과 폰이 멀어졌다는 뜻일 수 있습니다.
- 움직임 판단은 전부 기기가 합니다. 앱은 센서 값을 받지 않습니다.

## 2. BLE 인터페이스

HM-10은 BLE GATT 특성 하나를 **양방향 시리얼 통로**처럼 씁니다. 앱이 이 특성에 쓰면 기기가 받고, 기기가 보내면 앱에 Notify로 도착합니다.

| 항목 | 값 |
|---|---|
| BLE 모듈 | HM-10 (CC2541, 펌웨어 HMSoft) |
| 광고 이름 | `UMBRELLA` |
| Service UUID | `0000FFE0-0000-1000-8000-00805F9B34FB` |
| Characteristic UUID | `0000FFE1-0000-1000-8000-00805F9B34FB` |
| Characteristic 속성 | Read, Write Without Response, Notify (모듈에 따라 Write도 있음) |
| CCCD | `00002902-0000-1000-8000-00805F9B34FB` (Notify 켜기: `0x01 0x00`) |
| 페어링/본딩 | **필수.** 6자리 PIN 인증 후 본딩(HM-10 `TYPE3`). 페어링하지 않은 폰은 연결은 되어도 특성을 읽거나 쓸 수 없습니다(5.3.1). |
| PIN | 기기마다 다른 6자리 숫자. 기기와 함께 제공되는 **QR 라벨**에 들어 있습니다(4.4). **앱에 고정값으로 넣지 않습니다.** |
| MTU | **23 고정** (실제 데이터는 패킷당 20 바이트). MTU 협상을 하지 마세요. |
| 통신 범위 | 실내 약 10 m |

> **20 바이트 제한**: 한 번의 쓰기와 한 번의 Notify에 담기는 데이터는 최대 20 바이트입니다. 기기의 메시지 한 줄이 Notify 여러 개로 **쪼개져서 도착할 수 있고**, 반대로 여러 줄이 한 Notify에 **붙어서 올 수도 있습니다.** 앱은 반드시 줄 단위로 다시 조립해야 합니다(4.1).

## 3. 프로토콜 (proto=1)

### 3.1 형식

- ASCII 텍스트, **한 줄 = 메시지 하나**입니다.
- **앱 → 기기**: 명령 뒤에 `\n`을 붙입니다. 예: `ARM\n`. 대소문자는 구분하지 않습니다.
  - 줄바꿈 없이 보내도 기기는 100 ms 동안 추가 입력이 없으면 명령으로 처리합니다. 그래도 `\n`을 붙이는 것을 원칙으로 합니다.
  - 명령은 15자 이하입니다.
- **기기 → 앱**: 각 메시지는 `\r\n`으로 끝납니다. 앱은 `\n`을 기준으로 줄을 나누고, 끝의 `\r`과 공백을 제거합니다.
- 메시지는 `종류:값` 형태입니다. 앱은 **모르는 메시지를 무시**해야 합니다(하위 호환).

### 3.2 명령 (앱 → 기기)

| 명령 | 동작 | 응답 |
|---|---|---|
| `ARM` | 보관 모드 시작 | `OK:ARM` 다음 `STATE:SETTLING`. 약 3초 뒤 `STATE:ARMED` |
| `DISARM` | 보관 모드 또는 경보 해제 | `OK:DISARM` 다음 `STATE:DISARMED` |
| `STATUS` | 현재 상태 조회 | `STATE:<상태>` |
| `PING` | 연결 확인 | `PONG` |
| `INFO` | 기기 정보 | `INFO:fw=1.1.0,proto=1,sensor=ok` |

- `ARM`은 **멱등**입니다. 이미 SETTLING, ARMED, ALARM 상태에서 다시 보내면 `OK:ARM`과 현재 `STATE:`만 보내고 상태는 바뀌지 않습니다.
- `DISARM`은 어떤 상태에서도 DISARMED로 바꿉니다.
- 응답은 보통 **200 ms 안에** 옵니다. 1초 안에 응답이 없으면 한 번 다시 보내고, 그래도 없으면 연결 이상으로 처리합니다.

### 3.3 메시지 (기기 → 앱)

| 메시지 | 언제 | 앱이 할 일 |
|---|---|---|
| `STATE:DISARMED` | 해제됨, 부팅 직후, 연결 직후, STATUS 응답 | 상태 표시 갱신 |
| `STATE:SETTLING` | ARM 직후 3초 | "우산을 꽂아 두세요" 안내 |
| `STATE:ARMED` | 보관 모드 시작 | 상태 표시 |
| `STATE:ALARM` | 움직임 감지 | **경보 알림** (3.5) |
| `ALERT:MOVED` | ALARM 진입 시, 그리고 ALARM 동안 **2초마다 반복** | 경보 알림 (반복 수신 시 중복 알림 금지) |
| `OK:ARM`, `OK:DISARM` | 명령 수신 확인 | 대기 중인 명령 완료 처리 |
| `PONG` | PING 응답 | 연결 확인 |
| `INFO:k=v,k=v,...` | INFO 응답 | `proto` 값 확인 (3.6) |
| `ERR:SENSOR` | 센서 고장 상태에서 ARM | "기기 센서 오류" 표시. 무장 실패 |
| `ERR:UNKNOWN` | 알 수 없는 명령 | 로그만 남김 |

- **같은 `STATE:`가 연달아 올 수 있습니다.** 예를 들어 연결 직후 자동 전송과 STATUS 응답이 겹칠 때입니다. 앱은 상태가 실제로 바뀔 때만 반응하세요.
- 기기는 연결을 감지하면 `STATE:`를 한 번 자동으로 보냅니다. 하지만 앱이 Notify를 켜기 전에 보낸 메시지는 사라지므로, **앱은 Notify를 켠 직후 항상 `STATUS`를 보내야 합니다.**

### 3.4 기기 상태 머신

```
              ARM                  3 s 경과
 DISARMED ─────────▶ SETTLING ─────────────▶ ARMED
    ▲  ▲                 │                     │ 움직임 감지
    │  │      DISARM     │                     ▼
    │  └─────────────────┘                   ALARM ── 부저 + ALERT:MOVED (2 s 주기)
    └──────────────────── DISARM ──────────────┘
```

- **ALARM은 DISARM을 받을 때까지 계속됩니다.** 앱과 연결이 끊겨도 기기는 경보를 유지합니다. 다시 연결되면 앱은 `STATE:ALARM`을 받게 됩니다.
- 기기가 재부팅되면(전원 차단 등) 항상 DISARMED로 시작합니다. 앱이 ARMED라고 알고 있는데 재연결 후 `STATE:DISARMED`가 오면, "**기기가 재시작되어 보관 모드가 해제되었습니다**"라고 알려야 합니다.

### 3.5 시퀀스

**연결 및 초기화**
```
앱                                         기기
 │── connectGatt ────────────────────────▶│
 │── discoverServices ───────────────────▶│
 │── CCCD 쓰기 (Notify ON) ──────────────▶│
 │── "STATUS\n" ─────────────────────────▶│
 │◀───────────────────────── "STATE:ARMED" │
 │── "INFO\n" (선택, 최초 연결 시) ────────▶│
 │◀───────── "INFO:fw=1.1.0,proto=1,sensor=ok"
```

**보관 모드와 경보**
```
 │── "ARM\n" ────────────────────────────▶│
 │◀──────────────────────────────── "OK:ARM"
 │◀─────────────────────── "STATE:SETTLING"   (사용자는 우산을 꽂음)
 │◀────────────────────────── "STATE:ARMED"   (약 3초 뒤)
 │            ... 누군가 우산을 뽑음 ...
 │◀────────────────────────── "STATE:ALARM"
 │◀────────────────────────── "ALERT:MOVED"   → 시스템 알림 1회
 │◀────────────────────────── "ALERT:MOVED"   (2초마다 반복, 알림 갱신 안 함)
 │── "DISARM\n" ─────────────────────────▶│   (사용자가 알림/앱에서 해제)
 │◀───────────────────────────── "OK:DISARM"
 │◀─────────────────────── "STATE:DISARMED"   → 경보 알림 닫기
```

### 3.6 버전 호환

- 앱은 최초 연결 시 `INFO`를 보내 `proto` 값을 확인합니다.
- `proto`가 앱이 아는 버전보다 크면 "앱 업데이트 필요"를 안내하되, 아는 메시지는 계속 처리합니다.
- `INFO`에 응답하지 않는 기기(초기 펌웨어)는 proto=0으로 간주하고 `STATE:` 형식이 다를 수 있음을 로그로 남깁니다.

## 4. 앱 요구사항

### 4.1 필수 기능

| ID | 요구사항 |
|---|---|
| F-1 | **QR 등록 (기본 흐름)**: 사용자가 기기 라벨의 QR을 촬영하면, QR에 담긴 MAC 주소로 기기에 바로 연결하고 PIN으로 **페어링**(본딩)한 뒤 **등록**합니다(4.4, 5.3.1). 스캔 목록에서 기기를 고르는 단계는 없습니다. 등록한 기기의 MAC 주소를 저장하고, 이후에는 주소로 바로 연결합니다. |
| F-1b | **수동 등록 (대체 흐름)**: 카메라를 쓸 수 없거나 QR이 손상된 경우, 스캔 목록(이름 `UMBRELLA`)에서 라벨의 ID(MAC 끝 6자리)와 같은 기기를 고르고, 시스템 PIN 창에 라벨의 PIN 숫자를 입력합니다. |
| F-1a | 페어링에 실패하면(PIN 틀림, 취소) 등록하지 않고 "PIN을 확인하세요"라고 안내합니다. 본딩 정보가 사라진 경우(사용자가 시스템 설정에서 삭제, 기기 PIN 변경)에는 다시 등록하는 흐름(QR 촬영)으로 안내합니다. |
| F-2 | 연결 후 Notify를 켜고 `STATUS`를 보내 상태를 표시합니다. 표시 상태: 해제 / 준비 중 / 보관 중 / 경보 / 연결 안 됨 |
| F-3 | **보관 시작**(ARM)과 **해제**(DISARM) 버튼을 둡니다. 명령을 보낸 뒤 `OK:`와 `STATE:`를 받아야 완료로 표시합니다. |
| F-4 | 수신 데이터를 **줄 단위로 재조립**합니다. 바이트를 버퍼에 쌓고 `\n`이 나올 때마다 한 줄을 꺼냅니다. 한 줄이 256 바이트를 넘으면 깨진 줄로 보고 버퍼를 비운 뒤 **다음 `\n`까지 버립니다.** 비우기만 하면 깨진 줄의 나머지가 다음 메시지 앞에 붙어서 정상 메시지까지 잃게 됩니다. |
| F-5 | `STATE:ALARM` 또는 `ALERT:MOVED`를 받으면 **높은 우선순위 시스템 알림**(소리, 진동, 잠금 화면 표시)을 띄웁니다. 한 번의 경보에는 알림을 한 번만 띄웁니다. 알림에는 **"해제" 액션 버튼**을 넣어 바로 DISARM을 보낼 수 있게 합니다. |
| F-6 | **연결 끊김 경고**: 보관 모드(SETTLING, ARMED, ALARM) 중 연결이 끊기면 재연결을 시도합니다. **10초** 안에 재연결하지 못하면 "우산과 연결이 끊겼습니다" 알림을 띄웁니다. 재연결에 성공하면 알림을 닫고 `STATUS`로 상태를 다시 확인합니다. |
| F-7 | 앱이 백그라운드에 있거나 화면이 꺼져 있어도 보관 모드 동안 연결을 유지합니다(5.4). |
| F-8 | 재연결 후 `STATE:DISARMED`를 받았는데 앱은 보관 중으로 알고 있었다면 "기기가 재시작되어 보관이 해제되었습니다"라고 알립니다(3.4). |

### 4.2 권장 기능

| ID | 요구사항 |
|---|---|
| R-1 | 설정: 연결 끊김 경고 켜기/끄기, 대기 시간(5~60초) |
| R-2 | 통신 로그 화면(디버그용). 송수신 줄을 시간과 함께 표시 |
| R-3 | 연결 중 30초마다 `PING`을 보내 응답이 없으면 연결을 끊고 재연결 |

### 4.3 범위 밖 (v1)

- 여러 대 동시 관리, 기기 이름 변경, 펌웨어 업데이트, 센서 값 그래프, 서버나 클라우드 연동

### 4.4 QR 라벨

기기마다 QR 라벨이 하나씩 제공됩니다. 라벨은 설명서, 포장, 손잡이 안쪽처럼 **주인만 볼 수 있는 곳**에 둡니다. Matter 기기의 설정 코드와 같은 방식입니다.

```
┌──────────────────────┐
│    Umbrella Guard    │
│      ▓▓ QR ▓▓        │   QR 내용: UG1:884AEA72C66C:xxxxxx
│      ▓▓    ▓▓        │
│ UMBRELLA · ID 72C66C │   ID  = MAC 끝 6자리 (수동 등록 시 목록에서 구분용)
│    PIN  xxx xxx      │   PIN = 수동 입력용 숫자
└──────────────────────┘
```

**QR 데이터 형식 (UG1)**

```
UG1:<MAC>:<PIN>
```

| 필드 | 형식 | 예 | 설명 |
|---|---|---|---|
| `UG1` | 고정 문자열 | `UG1` | 형식 버전. 앱은 `UG1`만 받아들이고, 다른 값이면 "지원하지 않는 QR입니다. 앱을 업데이트하세요"라고 안내합니다. |
| MAC | 16진수 대문자 12자리, 구분자 없음 | `884AEA72C66C` | 기기 BLE 주소. Android 형식으로 바꾸려면 2자리마다 `:`를 넣습니다(`88:4A:EA:72:C6:6C`). |
| PIN | 숫자 6자리 | | 페어링 PIN |

- 검증용 정규식: `^UG1:([0-9A-F]{12}):(\d{6})$`. 앱은 인식한 문자열의 앞뒤 공백을 제거하고 대문자로 바꾼 뒤 검사합니다.
- 형식이 맞지 않으면 다른 QR로 보고 무시합니다(계속 촬영).
- **PIN은 등록이 끝날 때까지 메모리에만 두고, 저장하거나 로그에 남기지 않습니다.** 본딩이 끝나면 PIN은 다시 필요하지 않습니다.

## 5. 구현 지침 (Kotlin)

### 5.1 권한 (AndroidManifest.xml)

```xml
<!-- Android 12 (API 31) 이상 -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<!-- Android 11 이하 -->
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
<!-- 알림 (Android 13+) / 백그라운드 연결 유지 -->
<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />

<!-- QR 등록 -->
<uses-permission android:name="android.permission.CAMERA" />

<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
<uses-feature android:name="android.hardware.camera.any" android:required="false" />

<service
    android:name=".ble.GuardService"
    android:foregroundServiceType="connectedDevice"
    android:exported="false" />
```

### 5.2 상수

```kotlin
object Hm10 {
    val SERVICE: UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
    val CHAR: UUID    = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
    val CCCD: UUID    = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    const val MAX_PAYLOAD = 20
}
```

### 5.3 BLE 처리 핵심

1. **스캔**: `BluetoothLeScanner.startScan()`에 필터를 걸어 찾습니다. 스캔은 최대 10초로 제한합니다.
   - 기기는 광고에 Service UUID `FFE0`(Incomplete List of 16-bit Service UUIDs)과 이름 `UMBRELLA`(Complete Local Name)를 넣어 보냅니다.
   - 권장 필터: `ScanFilter.Builder().setServiceUuid(ParcelUuid(Hm10.SERVICE)).setDeviceName("UMBRELLA").build()`
   - 광고에는 이 밖에도 Manufacturer data(Company ID `0x4D48`, 값은 모듈 MAC 주소)와 Service Data(UUID `0xB000`)가 들어 있습니다. 이 둘은 HM-10 펌웨어가 자동으로 넣는 값이라 앱은 의존하지 않습니다.
2. **연결**: `device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)`
   - 처음 연결할 때는 `autoConnect = false`로 빠르게 연결합니다.
   - 백그라운드 재연결은 `autoConnect = true`로 연결을 맡겨 두면 기기가 범위에 돌아왔을 때 자동으로 붙습니다.
3. **GATT 작업은 한 번에 하나씩**: 서비스 검색 → CCCD 쓰기 → 특성 쓰기는 **반드시 이전 콜백을 받은 뒤** 다음 작업을 합니다. 명령 큐를 하나 두세요. Android BLE에서 가장 흔한 버그 원인입니다.
4. **Notify 켜기**: `setCharacteristicNotification(char, true)`를 호출하고, CCCD에 `ENABLE_NOTIFICATION_VALUE`를 씁니다. `onDescriptorWrite` 콜백이 오면 `STATUS\n`을 보냅니다.
5. **쓰기**: 쓰기 방식은 `WRITE_TYPE_NO_RESPONSE`를 씁니다. 특성 속성에 Write Without Response가 없으면 `WRITE_TYPE_DEFAULT`로 바꿉니다. 20 바이트를 넘는 데이터는 나눠서 보냅니다. v1 명령은 모두 20 바이트 이하입니다.
   - API 33 이상: `gatt.writeCharacteristic(char, bytes, writeType)`
   - 그 이하: `char.value = bytes; char.writeType = ...; gatt.writeCharacteristic(char)`
6. **수신**: `onCharacteristicChanged`에서 받은 바이트를 줄 버퍼에 추가합니다. API 33 이상은 `value` 파라미터가 있는 콜백을 씁니다.
7. **오류**: `status == 133`(GATT_ERROR) 같은 연결 실패가 나면 `gatt.close()` 후 1, 2, 4, 8초 간격으로 다시 시도합니다(최대 30초 간격). **연결을 끊을 때는 항상 `close()`까지 호출**하세요. 호출하지 않으면 연결 슬롯이 남아 이후 연결이 계속 실패합니다.

#### 5.3.1 PIN 페어링 (본딩)

기기는 PIN 인증과 본딩을 요구합니다(HM-10 `TYPE3`). 처리 순서는 다음과 같습니다.

1. **등록할 때 명시적으로 본딩합니다.**
   - **QR 등록**(F-1):
     1. QR을 읽어 MAC과 PIN을 얻습니다. 카메라 미리보기는 CameraX, QR 인식은 ML Kit Barcode Scanning(`FORMAT_QR_CODE`)을 권장합니다.
     2. `adapter.getRemoteDevice(mac)`으로 기기 객체를 얻습니다.
     3. 기기가 근처에 있는지 확인하기 위해 해당 MAC 필터(`ScanFilter.setDeviceAddress`)로 최대 10초 스캔합니다. 찾지 못하면 "기기를 가까이 두고 전원을 확인하세요"라고 안내합니다.
     4. `device.createBond()`를 호출합니다.
   - **수동 등록**(F-1b): 스캔 목록에서 기기를 고른 뒤 `createBond()`를 호출하고, 사용자가 시스템 PIN 창에 직접 입력합니다.
   - `BluetoothDevice.ACTION_BOND_STATE_CHANGED` 브로드캐스트를 받아 `BOND_BONDED`가 되면 성공입니다. `BOND_BONDING`에서 `BOND_NONE`으로 돌아가면 실패(PIN 틀림 또는 취소)입니다.
   - **QR 등록의 PIN 자동 입력**:
     - `BluetoothDevice.ACTION_PAIRING_REQUEST` 리시버를 높은 우선순위로 등록합니다.
     - 대상 기기이고 `EXTRA_PAIRING_VARIANT`가 PIN 방식이면 `device.setPin(pin.toByteArray())`로 입력합니다.
     - 브로드캐스트를 중단(`abortBroadcast()`)하면 시스템 창이 뜨지 않는 경우가 많지만, **제조사와 Android 버전에 따라 시스템 창이 그대로 뜨거나 자동 입력이 무시될 수 있습니다.**
   - **자동 입력이 안 될 때의 대체 흐름**: 시스템 PIN 창이 뜨면, 앱은 그 뒤나 알림 영역에 "PIN: xxx xxx"를 크게 보여 주고 클립보드에 복사해 사용자가 붙여 넣을 수 있게 합니다. 30초 안에 본딩되지 않으면 실패로 처리합니다.
   - 본딩에 성공한 뒤 GATT 연결을 시작합니다. 본딩 없이 연결하면 CCCD 쓰기나 특성 쓰기가 `GATT_INSUFFICIENT_AUTHENTICATION`(5) 또는 `GATT_INSUFFICIENT_ENCRYPTION`(15)으로 실패합니다.
2. **이후 연결에서는 PIN을 다시 묻지 않습니다.** 저장된 본딩 키로 자동 암호화됩니다. 연결 전에 `device.bondState == BOND_BONDED`인지 확인합니다.
3. **본딩이 사라진 경우**(사용자가 시스템 설정에서 기기를 삭제했거나 기기 쪽 PIN이나 모듈이 바뀐 경우): 연결 후 인증 오류가 나거나 `bondState`가 `BOND_NONE`이면, 사용자에게 다시 페어링하라고 안내하고 1번 흐름으로 돌아갑니다.
4. **PIN을 저장하지 않습니다.** 자동 입력은 QR 등록 중에만 쓰고, 등록이 끝나면(성공, 실패 모두) 메모리에서 지웁니다. 클립보드에 복사했다면 Android 13 이상에서는 `ClipDescription.EXTRA_IS_SENSITIVE`를 설정합니다.
5. 일부 기기에서는 `createBond()` 없이 연결만 해도 특성 접근 시점에 시스템이 PIN 창을 띄웁니다. 하지만 이 동작은 제조사마다 다르므로, **1번처럼 명시적으로 본딩하는 방식을 기준으로** 합니다.

> **보안 범위**: 본딩하면 링크가 암호화되고, PIN을 모르는 폰은 `ARM`/`DISARM`을 보낼 수 없습니다. 또 HM-10은 한 번에 폰 한 대만 연결되므로, 주인 폰이 연결되어 있는 동안에는 다른 폰이 접속할 수도 없습니다. PIN은 QR 라벨 실물로만 관리하므로, **라벨을 가진 사람만 등록할 수 있다**는 것이 v1의 보안 전제입니다.

**줄 재조립 예시**
```kotlin
class LineAssembler(private val onLine: (String) -> Unit) {
    private val buf = StringBuilder()
    private var discarding = false          // 너무 긴 줄: 다음 '\n' 까지 버리는 중
    fun feed(bytes: ByteArray) {
        for (b in bytes) {
            val c = (b.toInt() and 0xFF).toChar()
            if (c == '\n') {
                if (!discarding) {
                    val line = buf.toString().trim()
                    if (line.isNotEmpty()) onLine(line)
                }
                buf.clear()
                discarding = false
            } else if (!discarding) {
                buf.append(c)
                if (buf.length > 256) { buf.clear(); discarding = true }
            }
        }
    }
}
```

**메시지 해석 예시**
```kotlin
sealed interface DeviceMsg {
    data class State(val mode: Mode) : DeviceMsg
    data class Ok(val cmd: String) : DeviceMsg
    data class Err(val code: String) : DeviceMsg
    data class Info(val fields: Map<String, String>) : DeviceMsg
    object Alert : DeviceMsg
    object Pong : DeviceMsg
    data class Unknown(val raw: String) : DeviceMsg
}
enum class Mode { DISARMED, SETTLING, ARMED, ALARM }

fun parse(line: String): DeviceMsg {
    val (kind, value) = line.split(':', limit = 2).let { it[0] to it.getOrElse(1) { "" } }
    return when (kind) {
        "STATE" -> Mode.entries.find { it.name == value }?.let { DeviceMsg.State(it) } ?: DeviceMsg.Unknown(line)
        "OK"    -> DeviceMsg.Ok(value)
        "ERR"   -> DeviceMsg.Err(value)
        "INFO"  -> DeviceMsg.Info(value.split(',').mapNotNull {
                       it.split('=', limit = 2).takeIf { p -> p.size == 2 }?.let { p -> p[0] to p[1] } }.toMap())
        "ALERT" -> DeviceMsg.Alert
        "PONG"  -> DeviceMsg.Pong
        else    -> DeviceMsg.Unknown(line)
    }
}
```

### 5.4 백그라운드 동작

- 보관 모드 동안에는 **Foreground Service**(`foregroundServiceType="connectedDevice"`)가 BLE 연결을 들고 있어야 합니다. Activity가 연결을 들고 있으면 화면이 꺼질 때 끊깁니다.
  - 서비스 알림 예: "우산 보관 중 · 연결됨". 실제 연결과 `STATE`를 확인하기 전에는 "기기 연결 확인 중"처럼 사실과 다르지 않은 문구를 씁니다.
  - **시작 조건:** Android 12 이상에서는 `BLUETOOTH_CONNECT` 권한을 받은 뒤에만 서비스를 시작합니다. Android 14 이상에서 권한 없이 `connectedDevice` 타입으로 시작하면 `SecurityException`이 납니다.
  - 시스템이 서비스를 재시작(`START_STICKY`)하면 연결이 없는데도 상태 알림만 떠 있을 수 있습니다. 재시작을 허용하려면 재시작 직후 재연결을 시도하고, 그 결과에 따라 상태 알림과 F-8 안내를 처리해야 합니다.
- 보관을 해제하면(DISARMED) 서비스를 멈춰도 됩니다.
- 알림 채널은 두 개로 나눕니다.
  - `guard_status`: 상태 표시용, 중요도 LOW
  - `guard_alarm`: 경보와 연결 끊김용, 중요도 HIGH, 소리와 진동, 잠금 화면 공개
- Android 13 이상에서는 `POST_NOTIFICATIONS` 권한을 처음 ARM할 때 요청합니다.
- 일부 제조사(삼성 등)는 배터리 최적화 때문에 백그라운드 서비스를 종료합니다. 설정 화면에 "배터리 최적화 제외" 안내를 넣으세요.
- (선택) `CompanionDeviceManager`로 기기를 등록해 두면 백그라운드 실행 제한이 완화되고, 기기가 근처에 나타났을 때 앱이 깨어나도록 할 수 있습니다.

### 5.5 권장 구조

```
ui/        QrScanScreen (기본 등록), ScanScreen (수동 등록), DeviceScreen (상태, ARM/DISARM 버튼),
           SettingsScreen, LogScreen
ble/       GuardService (Foreground Service, 연결 소유)
           BleClient (스캔/연결/GATT 큐/재연결), LineAssembler, Protocol(parse, 명령 문자열)
data/      저장된 기기 주소, 설정 (DataStore)
notify/    알림 채널, 경보 알림 (해제 액션 → GuardService 로 DISARM 전달)
```
UI는 `GuardService`가 노출하는 `StateFlow<GuardUiState>`(연결 상태, 기기 모드, 마지막 수신 시각)를 구독합니다.

## 6. 테스트

### 6.1 기기 없이 개발할 때

다른 Android 폰에 **nRF Connect** 앱을 설치하고, "Configure GATT server"에서 Service `FFE0` 안에 특성 `FFE1`(Read, Write Without Response, Notify)을 만들면 기기를 흉내 낼 수 있습니다. 그 상태에서 `STATE:ARMED\r\n` 같은 값을 직접 Notify로 보내며 테스트합니다. 이 방식으로는 PIN 페어링까지 똑같이 재현하기 어려우니, 페어링 흐름은 실제 기기로 확인하세요.

### 6.2 실제 기기 인수 테스트

| # | 절차 | 기대 결과 |
|---|---|---|
| 1 | 라벨 QR 촬영 | 목록 선택 없이 연결, 본딩 완료(자동 입력 또는 대체 흐름), "해제" 상태 표시 |
| 1d | QR이 아닌 다른 QR(웹 주소 등), `UG2:` 형식 QR 촬영 | 무시하거나 "지원하지 않는 QR" 안내, 연결 시도 없음 |
| 1e | 기기 전원을 끈 상태에서 라벨 QR 촬영 | 약 10초 뒤 "기기를 가까이 두고 전원을 확인하세요" |
| 1f | 수동 등록: 스캔 목록에서 라벨 ID가 같은 `UMBRELLA` 선택, 라벨 PIN 입력 | 본딩 완료, 등록 |
| 1a | 수동 등록에서 틀린 PIN 입력 | 등록 실패, "PIN을 확인하세요" 안내, 저장된 기기 없음 |
| 1b | 페어링하지 않은 다른 폰(범용 BLE 앱)으로 연결해 `DISARM` 전송 | 쓰기 실패(인증 오류), 기기 상태 변화 없음 |
| 1c | 시스템 블루투스 설정에서 기기 삭제 후 앱 실행 | 다시 페어링하라는 안내 |
| 2 | 보관 시작 | "준비 중"이 표시되고 약 3초 뒤 "보관 중" |
| 3 | 보관 중 우산을 뽑음 | 기기 부저가 울리고, 폰에 경보 알림이 1회 뜸(잠금 화면 포함) |
| 4 | 알림의 "해제" 버튼 누름 | 부저가 멈추고 "해제" 상태, 경보 알림 닫힘 |
| 5 | 보관 중 폰을 들고 20 m 이상 멀어짐 | 약 10초 뒤 "연결 끊김" 알림 |
| 6 | 다시 가까이 옴 | 자동 재연결, 끊김 알림 닫힘, "보관 중" 표시 |
| 7 | 보관 중 앱을 백그라운드로 보내고 화면 끔, 5분 뒤 우산을 뽑음 | 경보 알림 수신 |
| 8 | 보관 중 기기 전원을 껐다 켬 | 재연결 후 "기기 재시작으로 보관 해제" 알림 |
| 9 | 연결 상태에서 앱 강제 종료 후 재실행 | 저장된 기기에 자동 연결되고 현재 상태 표시 |

기기 쪽 동작은 USB 시리얼 모니터(115200 baud)로도 확인할 수 있습니다. 같은 메시지가 출력되고, 시리얼로 `ARM`/`DISARM`을 입력해도 동작합니다.

## 7. 미정 사항과 확인 필요 항목

| 항목 | 상태 | 내용 |
|---|---|---|
| 보안 (DISARM 인증) | **결정됨** | HM-10 PIN 본딩(`TYPE3`)으로 처리합니다(5.3.1). 메시지 형식은 바뀌지 않으므로 proto=1을 유지합니다. |
| 광고 이름 | **결정됨** | `UMBRELLA`. 스캔은 이름으로 거르지만, 연결은 등록된 MAC 주소로 합니다. |
| PIN 전달 방법 | **결정됨** | 기기마다 QR 라벨(4.4)을 제공합니다. 라벨은 펌웨어 저장소의 `setup_ble`과 `tools/make_qr.py`로 만듭니다. |
| PIN 자동 입력 호환성 | 확인 필요 | `setPin()` 자동 입력이 동작하는지 대상 폰(제조사, Android 버전)별로 확인해야 합니다. 안 되는 폰은 대체 흐름(5.3.1)을 씁니다. |
| 광고에 FFE0 UUID 포함 여부 | **확인됨** | 광고에 `FFE0`이 포함되어 있습니다(HMSoft V608, nRF Connect로 확인). UUID와 이름으로 함께 스캔 필터를 겁니다(5.3). |
| 동시 연결 | 제약 | HM-10은 한 번에 폰 한 대만 연결됩니다. 다른 폰이 연결되어 있으면 스캔에 나타나지 않습니다. |
| 감지 감도 | 조정 중 | 기기 쪽 임계값은 실험으로 조정 중입니다. 앱에는 영향이 없습니다. |

## 8. 변경 이력

| 날짜 | 펌웨어 | proto | 내용 |
|---|---|---|---|
| 2026-11-25 | 1.0.0 | 1 | 최초 작성. `STATE:`/`OK:`/`ERR:` 형식 도입, `PING`/`INFO` 추가, 데이터 수신 즉시 연결 인식 |
| 2026-11-25 | 1.0.0 | 1 | 광고 이름 `UMBRELLA`, PIN 본딩 필수(5.3.1)로 변경. 메시지 형식은 그대로 |
| 2026-11-25 | 1.0.0 | 1 | 실기기 확인 결과 반영: 광고에 FFE0 포함, 스캔 필터 권장안 추가(5.3) |
| 2026-11-25 | 1.0.0 | 1 | QR 라벨 등록 추가: QR 형식 `UG1`(4.4), F-1을 QR 등록으로 변경하고 수동 등록 F-1b 추가, PIN 자동 입력과 대체 흐름(5.3.1), CAMERA 권한, 인수 테스트 1d~1f |
| 2026-11-26 | 1.1.0 | 1 | 기기 쪽 움직임 감지 방식 개선(툭 침 오경보 감소). 프로토콜과 앱 동작은 바뀌지 않음 |
| 2026-11-27 | 1.1.0 | 1 | 앱 골격 리뷰 반영: 권한에 `ACCESS_COARSE_LOCATION` 추가(5.1), 너무 긴 줄은 다음 `\n`까지 버리도록 F-4 변경, Foreground Service 시작 조건과 재시작 주의(5.4) |
