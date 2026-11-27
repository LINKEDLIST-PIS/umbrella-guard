package com.umbrellaguard.app.ble

enum class Connection { DISCONNECTED, CONNECTING, CONNECTED }

/** UI 가 구독하는 상태 (APP_SPEC 5.5). GuardService 가 소유한 BleClient 가 갱신한다. */
data class GuardUiState(
    val connection: Connection = Connection.DISCONNECTED,
    /** 마지막으로 받은 STATE. 연결 전이거나 아직 STATUS 응답이 없으면 null */
    val mode: Mode? = null,
    val lastRxAtMillis: Long? = null,
)
