package com.umbrellaguard.app.ble

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

/**
 * HM-10 연결 담당. GuardService 가 하나만 소유한다.
 *
 * 구현할 것 (APP_SPEC 5.3, 5.3.1):
 *  - 스캔: ScanFilter(Service UUID FFE0 + 이름 UMBRELLA), 최대 10 초
 *  - 연결: connectGatt(autoConnect = false, TRANSPORT_LE), 백그라운드 재연결은 autoConnect = true
 *  - GATT 작업 큐: 서비스 검색 -> CCCD 쓰기 -> 특성 쓰기를 콜백 받은 뒤 하나씩
 *  - Notify 켠 직후 STATUS 전송, 최초 연결 시 INFO 로 proto 확인
 *  - 쓰기: WRITE_TYPE_NO_RESPONSE (속성에 없으면 DEFAULT)
 *  - status 133 등 실패: close() 후 1, 2, 4, 8 초 ... 최대 30 초 간격 재시도. 끊을 때는 항상 close()
 *  - 본딩: QR 등록 시 createBond() + PIN 자동 입력, 실패 시 대체 흐름
 *  - 연결 끊김 경고: 보관 중 끊기면 10 초 안에 재연결 못 하면 알림 (F-6)
 */
@Suppress("unused") // TODO: 구현하면서 제거
class BleClient(private val context: Context) {

    private val _state = MutableStateFlow(GuardUiState())
    val state: StateFlow<GuardUiState> = _state.asStateFlow()

    private val assembler = LineAssembler { line -> onLine(line) }

    /** 등록된 MAC 주소로 연결 */
    fun connect(address: String) {
        // TODO(APP_SPEC 5.3): BluetoothAdapter.getRemoteDevice(address).connectGatt(...)
    }

    fun disconnect() {
        // TODO: gatt.disconnect() 후 gatt.close()
    }

    fun send(cmd: Command) {
        // TODO: GATT 작업 큐에 넣기. cmd.toBytes() 는 항상 Hm10.MAX_PAYLOAD 이하
    }

    /** BluetoothGattCallback.onCharacteristicChanged 에서 호출 */
    internal fun onNotify(bytes: ByteArray) = assembler.feed(bytes)

    private fun onLine(line: String) {
        val now = System.currentTimeMillis()
        when (val msg = Protocol.parse(line)) {
            is DeviceMsg.State -> _state.update { it.copy(mode = msg.mode, lastRxAtMillis = now) }
            is DeviceMsg.Alert -> _state.update { it.copy(mode = Mode.ALARM, lastRxAtMillis = now) }
            else -> _state.update { it.copy(lastRxAtMillis = now) }
            // TODO: Ok -> 대기 중인 명령 완료, Err -> 오류 표시, Info -> proto 확인 (APP_SPEC 3.6)
        }
    }
}
