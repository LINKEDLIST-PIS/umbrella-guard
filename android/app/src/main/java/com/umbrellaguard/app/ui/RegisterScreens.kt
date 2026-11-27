package com.umbrellaguard.app.ui

import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.umbrellaguard.app.ble.QrPayload

/**
 * QR 등록 화면 (기본 흐름, APP_SPEC F-1, 4.4, 5.3.1).
 *
 * TODO:
 *  - CameraX 미리보기 + ML Kit BarcodeScanning(FORMAT_QR_CODE)
 *  - 인식한 문자열을 QrPayload.parse() 로 검사
 *      Ok -> onResult, UnsupportedVersion -> "앱을 업데이트하세요", NotOurs -> 무시하고 계속 촬영
 *  - CAMERA 권한 요청, 거부 시 수동 등록으로 안내
 */
@Composable
fun QrScanScreen(
    @Suppress("UNUSED_PARAMETER") onResult: (QrPayload) -> Unit,
    modifier: Modifier = Modifier,
) {
    Text("TODO: QR 촬영 (APP_SPEC 4.4)", modifier.padding(24.dp))
}

/**
 * 수동 등록 화면 (대체 흐름, APP_SPEC F-1b).
 *
 * TODO: 이름 UMBRELLA 스캔 목록 + 라벨 ID(MAC 끝 6자리) 표시, 선택 시 createBond()
 */
@Composable
fun ScanScreen(
    @Suppress("UNUSED_PARAMETER") onSelect: (address: String) -> Unit,
    modifier: Modifier = Modifier,
) {
    Text("TODO: 수동 등록 (APP_SPEC F-1b)", modifier.padding(24.dp))
}
