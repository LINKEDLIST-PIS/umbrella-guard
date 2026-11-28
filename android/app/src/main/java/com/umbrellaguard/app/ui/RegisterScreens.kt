package com.umbrellaguard.app.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.umbrellaguard.app.R
import com.umbrellaguard.app.ble.QrPayload

/** QR scanner integration point (APP_SPEC F-1, 4.4, 5.3.1). */
@Composable
fun QrScanScreen(
    @Suppress("UNUSED_PARAMETER") onResult: (QrPayload) -> Unit,
    modifier: Modifier = Modifier,
    onManualRegister: (() -> Unit)? = null,
    onBack: (() -> Unit)? = null,
) {
    // TODO: Connect CameraX and QR parsing to onResult.
    QrRegistrationContent(
        modifier = modifier,
        onManualRegister = onManualRegister,
        onBack = onBack,
    )
}

/** BLE discovery integration point (APP_SPEC F-1b). */
@Composable
fun ScanScreen(
    onSelect: (address: String) -> Unit,
    modifier: Modifier = Modifier,
    onBack: (() -> Unit)? = null,
) {
    // TODO: Pass real scan results and a scan callback from the BLE layer.
    ScanRegistrationContent(
        devices = emptyList(),
        onSelect = onSelect,
        modifier = modifier,
        onBack = onBack,
    )
}


@Composable
private fun RegistrationHeader(onBack: (() -> Unit)?) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        if (onBack != null) {
            IconButton(onClick = onBack) {
                Icon(
                    painter = painterResource(R.drawable.ic_arrow_back),
                    contentDescription = stringResource(R.string.action_back),
                )
            }
        }
        Text("우산 등록", style = MaterialTheme.typography.headlineSmall)
    }
}
/** 화면에 필요한 표시용 데이터. 실제 BLE 검색 객체와는 분리한다. */
data class RegistrationDevice(
    val address: String,
    val name: String = "UMBRELLA",
) {
    val labelId: String get() = address.replace(":", "").takeLast(6).uppercase()
}

/** 카메라 미리보기와 수동 등록 콜백을 나중에 주입할 수 있는 순수 UI. */
@Composable
fun QrRegistrationContent(
    modifier: Modifier = Modifier,
    cameraContent: (@Composable () -> Unit)? = null,
    onManualRegister: (() -> Unit)? = null,
    message: String? = null,
    onBack: (() -> Unit)? = null,
) {
    Column(
        modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        RegistrationHeader(onBack)
        Text("QR 코드로 등록", style = MaterialTheme.typography.titleLarge)
        Text("우산의 QR 코드를 화면 안에 맞춰 주세요.",
            style = MaterialTheme.typography.bodyMedium)

        OutlinedCard(
            modifier = Modifier.fillMaxWidth().aspectRatio(1f),
            border = BorderStroke(2.dp, MaterialTheme.colorScheme.primary),
            colors = CardDefaults.outlinedCardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant,
            ),
        ) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                if (cameraContent != null) {
                    cameraContent()
                } else {
                    Column(
                        modifier = Modifier.padding(16.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text("QR 촬영 영역", style = MaterialTheme.typography.titleMedium)
                        Text("카메라 기능 연동 전", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
        if (message != null) {
            Text(message, color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodyMedium)
        }
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("등록 안내", style = MaterialTheme.typography.titleMedium)
                Text("1. 우산의 QR 코드를 찾습니다.")
                Text("2. 카메라로 코드를 인식합니다.")
                Text("3. 기기 등록 안내에 따라 진행합니다.")
            }
        }
        OutlinedButton(
            onClick = { onManualRegister?.invoke() },
            enabled = onManualRegister != null,
            modifier = Modifier.fillMaxWidth(),
        ) { Text("수동 등록") }
        if (onManualRegister == null) {
            Text("수동 등록 화면 이동 기능은 연동 예정입니다.",
                style = MaterialTheme.typography.bodySmall)
        }
    }
}

/** 스캔 결과와 콜백을 받기만 한다. 여기에서는 BLE 검색 또는 페어링을 실행하지 않는다. */
@Composable
fun ScanRegistrationContent(
    devices: List<RegistrationDevice>,
    onSelect: (address: String) -> Unit,
    modifier: Modifier = Modifier,
    scanning: Boolean = false,
    onScan: (() -> Unit)? = null,
    message: String? = null,
    onBack: (() -> Unit)? = null,
) {
    Column(
        modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        RegistrationHeader(onBack)
        Text("수동 기기 등록", style = MaterialTheme.typography.titleLarge)
        Text("주변의 UMBRELLA 기기를 찾고 우산 라벨의 ID와 비교하세요.",
            style = MaterialTheme.typography.bodyMedium)
        Button(
            onClick = { onScan?.invoke() },
            enabled = onScan != null && !scanning,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(if (scanning) "검색 중" else "기기 검색") }

        if (scanning) {
            Row(verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                CircularProgressIndicator()
                Text("주변 기기를 검색하고 있습니다.")
            }
        }
        if (message != null) {
            Text(message, color = MaterialTheme.colorScheme.error)
        }
        Text("검색된 기기 (${devices.size})", style = MaterialTheme.typography.titleMedium)
        if (devices.isEmpty() && !scanning) {
            OutlinedCard(modifier = Modifier.fillMaxWidth()) {
                Text(
                    if (onScan == null) "블루투스 검색 기능 연동 전" else "검색된 기기가 없습니다.",
                    modifier = Modifier.padding(20.dp),
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }
        devices.forEach { device ->
            OutlinedCard(modifier = Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(device.name, style = MaterialTheme.typography.titleMedium)
                    Text("라벨 ID: ${device.labelId}", style = MaterialTheme.typography.bodyLarge)
                    Text("MAC: ${device.address}", style = MaterialTheme.typography.bodySmall)
                    OutlinedButton(
                        onClick = { onSelect(device.address) },
                        enabled = !scanning,
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("이 우산 선택") }
                }
            }
        }
        Text("등록 전 우산 라벨과 검색된 기기의 ID가 같은지 확인하세요.",
            style = MaterialTheme.typography.bodySmall)
    }
}

@Preview(showBackground = true)
@Composable
private fun QrRegistrationPreview() {
    MaterialTheme { QrRegistrationContent(onManualRegister = {}) }
}

@Preview(showBackground = true)
@Composable
private fun ScanRegistrationPreview() {
    // 예시 데이터는 미리보기에만 사용. 실제 검색 화면에는 표시하지 않는다.
    MaterialTheme {
        ScanRegistrationContent(
            devices = listOf(RegistrationDevice("00:00:00:A1:B2:C3")),
            onSelect = {},
            onScan = {},
        )
    }
}
