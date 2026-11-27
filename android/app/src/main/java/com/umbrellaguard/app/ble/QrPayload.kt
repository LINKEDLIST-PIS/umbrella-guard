package com.umbrellaguard.app.ble

/**
 * 기기 QR 라벨 내용 (APP_SPEC 4.4).  형식: `UG1:<MAC 12자리 대문자 16진수>:<PIN 6자리>`
 *
 * PIN 은 등록이 끝날 때까지 메모리에만 두고, 저장하거나 로그에 남기지 않는다.
 * [toString] 은 PIN 을 가린다. 등록이 끝나면(성공/실패 모두) [clear] 를 호출한다.
 */
class QrPayload private constructor(
    /** Android 형식 MAC 주소 (예: 88:4A:EA:72:C6:6C) */
    val mac: String,
    private val pin: CharArray,
) {
    /** 라벨에 인쇄된 기기 ID (MAC 끝 6자리) */
    val deviceId: String get() = mac.replace(":", "").takeLast(6)

    /** BluetoothDevice.setPin() 에 넘길 값 */
    fun pinBytes(): ByteArray = String(pin).toByteArray(Charsets.US_ASCII)

    /** 대체 흐름(시스템 PIN 창에 직접 입력)에서 화면에 보여 줄 값. 예: "123 456" */
    fun pinForDisplay(): String = String(pin).let { "${it.substring(0, 3)} ${it.substring(3)}" }

    fun clear() = pin.fill('0')

    override fun toString(): String = "QrPayload(mac=$mac, pin=******)"

    companion object {
        private val UG1 = Regex("^UG1:([0-9A-F]{12}):(\\d{6})$")
        private val ANY_UG = Regex("^UG(\\d+):.*$")

        /** 앞뒤 공백 제거, 대문자 변환 후 검사한다. */
        fun parse(raw: String): QrParseResult {
            val s = raw.trim().uppercase()
            val m = UG1.matchEntire(s)
            if (m != null) {
                val mac = m.groupValues[1].chunked(2).joinToString(":")
                return QrParseResult.Ok(QrPayload(mac, m.groupValues[2].toCharArray()))
            }
            val other = ANY_UG.matchEntire(s)
            return if (other != null && other.groupValues[1] != "1") {
                QrParseResult.UnsupportedVersion   // "지원하지 않는 QR입니다. 앱을 업데이트하세요"
            } else {
                QrParseResult.NotOurs              // 다른 QR 또는 깨진 UG1 -> 무시하고 계속 촬영
            }
        }
    }
}

sealed interface QrParseResult {
    data class Ok(val payload: QrPayload) : QrParseResult
    data object UnsupportedVersion : QrParseResult
    data object NotOurs : QrParseResult
}
