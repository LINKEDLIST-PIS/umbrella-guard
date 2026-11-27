package com.umbrellaguard.app.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** 펌웨어 fw 1.1.0 / proto 1 의 실제 출력 형식 기준 (APP_SPEC 3장) */
class ProtocolTest {

    @Test fun parsesStates() {
        assertEquals(DeviceMsg.State(Mode.DISARMED), Protocol.parse("STATE:DISARMED"))
        assertEquals(DeviceMsg.State(Mode.SETTLING), Protocol.parse("STATE:SETTLING"))
        assertEquals(DeviceMsg.State(Mode.ARMED), Protocol.parse("STATE:ARMED"))
        assertEquals(DeviceMsg.State(Mode.ALARM), Protocol.parse("STATE:ALARM"))
    }

    @Test fun parsesOtherMessages() {
        assertEquals(DeviceMsg.Ok("ARM"), Protocol.parse("OK:ARM"))
        assertEquals(DeviceMsg.Err("SENSOR"), Protocol.parse("ERR:SENSOR"))
        assertEquals(DeviceMsg.Alert, Protocol.parse("ALERT:MOVED"))
        assertEquals(DeviceMsg.Pong, Protocol.parse("PONG"))
    }

    @Test fun parsesInfo() {
        val info = Protocol.parse("INFO:fw=1.1.0,proto=1,sensor=ok") as DeviceMsg.Info
        assertEquals("1.1.0", info.fw)
        assertEquals(1, info.proto)
        assertTrue(info.sensorOk)
    }

    @Test fun unknownIsIgnorable() {
        assertTrue(Protocol.parse("STATE:SLEEPING") is DeviceMsg.Unknown)
        assertTrue(Protocol.parse("HELLO:x") is DeviceMsg.Unknown)
    }

    @Test fun commandsAreShortLines() {
        assertArrayEquals("ARM\n".toByteArray(), Command.ARM.toBytes())
        Command.entries.forEach { assertTrue(it.toBytes().size <= Hm10.MAX_PAYLOAD) }
    }

    @Test fun reassemblesSplitAndJoinedNotifications() {
        val lines = mutableListOf<String>()
        val a = LineAssembler { lines += it }
        // 한 줄이 쪼개져 오는 경우
        a.feed("STATE:AR".toByteArray())
        a.feed("MED\r\n".toByteArray())
        // 여러 줄이 한 Notify 에 붙어 오는 경우
        a.feed("OK:DISARM\r\nSTATE:DISARMED\r\n".toByteArray())
        assertEquals(listOf("STATE:ARMED", "OK:DISARM", "STATE:DISARMED"), lines)
    }

    @Test fun dropsOverlongLineUntilNewline() {
        val lines = mutableListOf<String>()
        val a = LineAssembler(maxLen = 8) { lines += it }
        // 8 바이트를 넘는 깨진 줄은 다음 '\n' 까지 통째로 버린다 (나머지가 다음 메시지에 붙지 않음)
        a.feed("XXXXXXXXXXXX".toByteArray())
        a.feed("XX\r\n".toByteArray())
        a.feed("PONG\r\n".toByteArray())
        assertEquals(listOf("PONG"), lines)
    }

    @Test fun recoversAfterOverlongLineInSameChunk() {
        val lines = mutableListOf<String>()
        val a = LineAssembler(maxLen = 8) { lines += it }
        // maxLen 8 이므로 정상 메시지도 8 바이트 이하("OK:ARM\r" = 7)로 확인
        a.feed("XXXXXXXXXXXXXX\r\nOK:ARM\r\n".toByteArray())
        assertEquals(listOf("OK:ARM"), lines)
    }
}
