package com.umbrellaguard.app.ble

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/** QR 형식 UG1 (APP_SPEC 4.4). 테스트용 PIN 은 실제 기기와 무관한 값. */
class QrPayloadTest {

    @Test fun parsesValidPayload() {
        val r = QrPayload.parse("UG1:884AEA72C66C:918273") as QrParseResult.Ok
        assertEquals("88:4A:EA:72:C6:6C", r.payload.mac)
        assertEquals("72C66C", r.payload.deviceId)
        assertEquals("918 273", r.payload.pinForDisplay())
        assertEquals("918273", String(r.payload.pinBytes()))
    }

    @Test fun trimsAndUppercases() {
        assertTrue(QrPayload.parse("  ug1:884aea72c66c:918273 \n") is QrParseResult.Ok)
    }

    @Test fun toStringHidesPin() {
        val r = QrPayload.parse("UG1:884AEA72C66C:918273") as QrParseResult.Ok
        assertFalse(r.payload.toString().contains("918273"))
    }

    @Test fun clearWipesPin() {
        val p = (QrPayload.parse("UG1:884AEA72C66C:918273") as QrParseResult.Ok).payload
        p.clear()
        assertEquals("000000", String(p.pinBytes()))
    }

    @Test fun rejectsOtherVersionsAndForeignCodes() {
        assertEquals(QrParseResult.UnsupportedVersion, QrPayload.parse("UG2:884AEA72C66C:918273"))
        assertEquals(QrParseResult.NotOurs, QrPayload.parse("https://example.com"))
        assertEquals(QrParseResult.NotOurs, QrPayload.parse("UG1:884AEA72C66:918273"))   // MAC 11자리
        assertEquals(QrParseResult.NotOurs, QrPayload.parse("UG1:884AEA72C66C:12345"))   // PIN 5자리
    }
}
