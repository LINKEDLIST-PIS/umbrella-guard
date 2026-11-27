package com.umbrellaguard.app.ble

import java.util.UUID

/** HM-10 BLE 인터페이스 상수 (APP_SPEC 2장, 5.2). */
object Hm10 {
    val SERVICE: UUID = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb")
    val CHAR: UUID = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb")
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    /** MTU 23 고정 -> 패킷당 데이터 최대 20 바이트. MTU 협상을 하지 않는다. */
    const val MAX_PAYLOAD = 20

    /** 광고 이름 (Complete Local Name). 연결은 이름이 아니라 등록된 MAC 주소로 한다. */
    const val DEVICE_NAME = "UMBRELLA"

    /** 스캔 최대 시간 (APP_SPEC 5.3) */
    const val SCAN_TIMEOUT_MS = 10_000L
}
