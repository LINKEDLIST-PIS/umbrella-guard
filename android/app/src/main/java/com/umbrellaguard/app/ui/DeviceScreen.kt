package com.umbrellaguard.app.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.umbrellaguard.app.R
import com.umbrellaguard.app.ble.Connection
import com.umbrellaguard.app.ble.GuardUiState
import com.umbrellaguard.app.ble.Mode

/** APP_SPEC F-2, F-3: 기존 상태와 콜백을 유지하는 화면. */
@Composable
fun DeviceScreen(
    state: GuardUiState,
    onArm: () -> Unit,
    onDisarm: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val connected = state.connection == Connection.CONNECTED
    val alarm = connected && state.mode == Mode.ALARM
    val canArm = connected && state.mode == Mode.DISARMED
    val canDisarm = connected && (state.mode == Mode.SETTLING ||
        state.mode == Mode.ARMED || state.mode == Mode.ALARM)
    val colors = MaterialTheme.colorScheme
    val statusColor = when {
        alarm -> colors.errorContainer
        connected && state.mode == Mode.ARMED -> colors.primaryContainer
        connected && state.mode == Mode.SETTLING -> colors.tertiaryContainer
        else -> colors.surfaceVariant
    }
    val statusInk = when {
        alarm -> colors.onErrorContainer
        connected && state.mode == Mode.ARMED -> colors.onPrimaryContainer
        connected && state.mode == Mode.SETTLING -> colors.onTertiaryContainer
        else -> colors.onSurfaceVariant
    }

    Column(
        modifier = modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text(stringResource(R.string.app_name), style = MaterialTheme.typography.headlineSmall)
        Text("우산 보관 관리", style = MaterialTheme.typography.bodyLarge)

        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = statusColor, contentColor = statusInk),
        ) {
            Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("현재 우산 상태", style = MaterialTheme.typography.labelLarge)
                Text(statusText(state), style = MaterialTheme.typography.headlineMedium)
                Text(when {
                    !connected -> "연결 상태를 확인하세요. 현재 우산 상태는 확인할 수 없습니다."
                    state.mode == Mode.ALARM -> "경보가 발생했습니다. 우산의 상태를 확인하세요."
                    state.mode == Mode.ARMED -> "보관 모드가 작동 중입니다."
                    state.mode == Mode.SETTLING -> "경계 모드를 준비하고 있습니다."
                    state.mode == Mode.DISARMED -> "우산을 보관하려면 보관 시작을 누르세요."
                    else -> "기기의 상태 응답을 기다리고 있습니다."
                }, style = MaterialTheme.typography.bodyMedium)
            }
        }

        OutlinedCard(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("블루투스 연결", style = MaterialTheme.typography.titleMedium)
                Text(if (connected) "연결됨" else "연결 확인 필요")
                Text("연결 상태와 기기 응답에 따라 버튼이 활성화됩니다.",
                    style = MaterialTheme.typography.bodySmall)
            }
        }

        Text("보관 모드 제어", style = MaterialTheme.typography.titleMedium)
        Button(onClick = onArm, enabled = canArm, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.action_arm))
        }
        OutlinedButton(onClick = onDisarm, enabled = canDisarm, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.action_disarm))
        }
        if (alarm) {
            Text("경보 해제는 보관 해제 버튼을 이용하세요.", color = colors.error,
                style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun statusText(state: GuardUiState): String = when {
    state.connection != Connection.CONNECTED -> stringResource(R.string.state_disconnected)
    else -> when (state.mode) {
        Mode.DISARMED -> stringResource(R.string.state_disarmed)
        Mode.SETTLING -> stringResource(R.string.state_settling)
        Mode.ARMED -> stringResource(R.string.state_armed)
        Mode.ALARM -> stringResource(R.string.state_alarm)
        null -> stringResource(R.string.state_unknown)
    }
}

@Preview(showBackground = true)
@Composable
private fun DeviceScreenPreview() {
    MaterialTheme {
        DeviceScreen(state = GuardUiState(connection = Connection.CONNECTED, mode = Mode.ARMED),
            onArm = {}, onDisarm = {})
    }
}

@Preview(showBackground = true)
@Composable
private fun DeviceAlarmPreview() {
    MaterialTheme {
        DeviceScreen(state = GuardUiState(connection = Connection.CONNECTED, mode = Mode.ALARM),
            onArm = {}, onDisarm = {})
    }
}
