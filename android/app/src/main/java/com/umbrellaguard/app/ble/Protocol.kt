package com.umbrellaguard.app.ble

/**
 * 기기 <-> 앱 텍스트 프로토콜 (APP_SPEC 3장, proto=1).
 *
 * - 앱 -> 기기: "명령\n"
 * - 기기 -> 앱: "종류:값\r\n". 모르는 메시지는 무시한다(하위 호환).
 */
object Protocol {
    /** 앱이 이해하는 프로토콜 버전. INFO 의 proto 값이 이보다 크면 업데이트 안내 (APP_SPEC 3.6). */
    const val VERSION = 1

    fun parse(line: String): DeviceMsg {
        val idx = line.indexOf(':')
        val kind = if (idx >= 0) line.substring(0, idx) else line
        val value = if (idx >= 0) line.substring(idx + 1) else ""
        return when (kind) {
            "STATE" -> Mode.entries.firstOrNull { it.name == value }
                ?.let { DeviceMsg.State(it) } ?: DeviceMsg.Unknown(line)
            "OK" -> DeviceMsg.Ok(value)
            "ERR" -> DeviceMsg.Err(value)
            "INFO" -> DeviceMsg.Info(parseFields(value))
            "ALERT" -> DeviceMsg.Alert
            "PONG" -> DeviceMsg.Pong
            else -> DeviceMsg.Unknown(line)
        }
    }

    private fun parseFields(value: String): Map<String, String> =
        value.split(',').mapNotNull { part ->
            val kv = part.split('=', limit = 2)
            if (kv.size == 2) kv[0] to kv[1] else null
        }.toMap()
}

/** 기기 상태 (APP_SPEC 3.4) */
enum class Mode { DISARMED, SETTLING, ARMED, ALARM }

/** 기기 -> 앱 메시지 (APP_SPEC 3.3) */
sealed interface DeviceMsg {
    data class State(val mode: Mode) : DeviceMsg
    data class Ok(val cmd: String) : DeviceMsg
    data class Err(val code: String) : DeviceMsg
    data class Info(val fields: Map<String, String>) : DeviceMsg {
        val fw: String? get() = fields["fw"]
        val proto: Int? get() = fields["proto"]?.toIntOrNull()
        val sensorOk: Boolean get() = fields["sensor"] == "ok"
    }
    data object Alert : DeviceMsg
    data object Pong : DeviceMsg
    data class Unknown(val raw: String) : DeviceMsg
}

/** 앱 -> 기기 명령 (APP_SPEC 3.2). 모두 20 바이트 이하라 한 번에 쓴다. */
enum class Command {
    ARM, DISARM, STATUS, PING, INFO;

    fun toBytes(): ByteArray = "$name\n".toByteArray(Charsets.US_ASCII)
}

/**
 * Notify 로 쪼개지거나 붙어서 오는 바이트를 줄 단위로 재조립한다 (APP_SPEC F-4).
 * '\n' 기준으로 자르고 앞뒤 공백과 '\r' 을 제거한다.
 *
 * 한 줄이 [maxLen] 을 넘으면 깨진 줄로 보고 버퍼를 비운 뒤 **다음 '\n' 까지 버린다.**
 * (비우기만 하면 깨진 줄의 나머지가 다음 메시지 앞에 붙어 정상 메시지까지 잃는다.)
 */
class LineAssembler(
    private val maxLen: Int = 256,
    private val onLine: (String) -> Unit,
) {
    private val buf = StringBuilder()
    private var discarding = false

    fun feed(bytes: ByteArray) {
        for (b in bytes) {
            val c = (b.toInt() and 0xFF).toChar()
            if (c == '\n') {
                if (!discarding) {
                    val line = buf.toString().trim()
                    if (line.isNotEmpty()) onLine(line)
                }
                buf.setLength(0)
                discarding = false
            } else if (!discarding) {
                buf.append(c)
                if (buf.length > maxLen) {
                    buf.setLength(0)
                    discarding = true
                }
            }
        }
    }
}
