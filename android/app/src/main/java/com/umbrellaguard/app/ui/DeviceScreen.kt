package com.umbrellaguard.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.umbrellaguard.app.R
import com.umbrellaguard.app.ble.Connection
import com.umbrellaguard.app.ble.GuardUiState
import com.umbrellaguard.app.ble.Mode

/** 기기 상태와 보관 시작/해제 버튼 (APP_SPEC F-2, F-3) */
@Composable
fun DeviceScreen(
    state: GuardUiState,
    onArm: () -> Unit,
    onDisarm: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val connected = state.connection == Connection.CONNECTED
    Column(
        modifier = modifier.padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Text(stringResource(R.string.app_name), style = MaterialTheme.typography.headlineSmall)
        Text(statusText(state), style = MaterialTheme.typography.titleLarge)
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(onClick = onArm, enabled = connected && state.mode == Mode.DISARMED) {
                Text(stringResource(R.string.action_arm))
            }
            // STATUS 응답 전(mode == null)에는 상태를 모르므로 비활성. 경보 중 해제는 알림 액션(F-5)으로도 가능
            val armedLike = state.mode == Mode.SETTLING || state.mode == Mode.ARMED || state.mode == Mode.ALARM
            OutlinedButton(onClick = onDisarm, enabled = connected && armedLike) {
                Text(stringResource(R.string.action_disarm))
            }
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
        DeviceScreen(
            state = GuardUiState(connection = Connection.CONNECTED, mode = Mode.ARMED),
            onArm = {},
            onDisarm = {},
        )
    }
}
