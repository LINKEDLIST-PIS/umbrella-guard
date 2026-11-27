# Umbrella Guard Android 앱

우산 도난 감지 기기와 BLE로 연결하는 Android 앱입니다. **앱이 지켜야 할 규약과 요구사항은 [`../docs/APP_SPEC.md`](../docs/APP_SPEC.md)가 기준입니다.** 이 폴더는 그 명세에 맞춘 시작용 골격입니다.

## 열기와 빌드

1. Android Studio에서 **Open**을 누르고 이 `android/` 폴더를 엽니다. 저장소 루트를 열면 안 됩니다.
2. Gradle Sync가 끝나면 `app`을 실행합니다. BLE는 에뮬레이터에서 동작하지 않으므로 실제 폰을 쓰세요.
3. 단위 테스트는 다음 명령으로 실행합니다.
   ```bash
   ./gradlew :app:testDebugUnitTest      # Windows: gradlew.bat :app:testDebugUnitTest
   ```

| 항목 | 값 |
|---|---|
| 언어 / UI | Kotlin, Jetpack Compose (Material 3) |
| minSdk / targetSdk | 26 / 35 |
| JDK | 17~21 (Gradle 8.11.1은 그보다 새 JDK에서 실행되지 않습니다. 최신 Android Studio의 내장 JBR이 21보다 새 버전이면 **Settings → Build Tools → Gradle → Gradle JDK**에서 17~21을 고르세요.) |
| 패키지 | `com.umbrellaguard.app` (필요하면 바꾸세요) |

## 구조와 명세 대응

```
app/src/main/java/com/umbrellaguard/app/
├─ ble/
│  ├─ Hm10.kt          UUID, 20바이트 제한, 광고 이름            [구현됨]  APP_SPEC 2, 5.2
│  ├─ Protocol.kt      메시지 파싱, 명령, 줄 재조립               [구현됨]  APP_SPEC 3, F-4
│  ├─ QrPayload.kt     QR 형식 UG1 파싱 (PIN 가림, 지우기)        [구현됨]  APP_SPEC 4.4
│  ├─ GuardUiState.kt  UI 상태
│  ├─ BleClient.kt     스캔, 연결, GATT 큐, 재연결, 본딩          [골격]    APP_SPEC 5.3, 5.3.1
│  └─ GuardService.kt  Foreground Service (연결 소유)             [골격]    APP_SPEC 5.4
├─ notify/Notifications.kt  알림 채널 2개                         [채널 구현, 경보 알림 TODO]
├─ data/DeviceStore.kt      등록된 MAC 저장 (PIN은 저장 안 함)    [구현됨]
├─ ui/DeviceScreen.kt       상태, 보관 시작/해제 버튼             [화면만]  F-2, F-3
├─ ui/RegisterScreens.kt    QR 등록, 수동 등록                    [골격]    F-1, F-1b
└─ MainActivity.kt, UmbrellaGuardApp.kt
app/src/test/                Protocol, QrPayload 단위 테스트
```

- `AndroidManifest.xml`의 권한은 APP_SPEC 5.1과 같습니다.
- `[골격]`인 파일에는 구현할 내용을 명세 절 번호와 함께 `TODO`로 적어 두었습니다.

## 기기 없이 개발하기

다른 Android 폰의 nRF Connect로 기기를 흉내 낼 수 있습니다(APP_SPEC 6.1). 실제 기기와 연결해 보려면 펌웨어 저장소 루트의 README를 참고해 보드를 준비하고, 기기 QR 라벨을 받으세요.
