package com.umbrellaguard.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.umbrellaguard.app.ble.GuardUiState
import com.umbrellaguard.app.ui.DeviceScreen
import com.umbrellaguard.app.ui.QrScanScreen
import com.umbrellaguard.app.ui.ScanScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(Modifier.fillMaxSize()) {
                    GuardScreens(Modifier.safeDrawingPadding())
                }
            }
        }
    }
}

private enum class Screen { DEVICE, QR, SCAN }

@Composable
private fun GuardScreens(modifier: Modifier = Modifier) {
    var screen by rememberSaveable { mutableStateOf(Screen.DEVICE) }

    fun goBack() {
        screen = when (screen) {
            Screen.DEVICE, Screen.QR -> Screen.DEVICE
            Screen.SCAN -> Screen.QR
        }
    }

    BackHandler(enabled = screen != Screen.DEVICE, onBack = ::goBack)

    when (screen) {
        Screen.DEVICE -> Column(modifier.fillMaxSize()) {
            OutlinedButton(
                onClick = { screen = Screen.QR },
                modifier = Modifier.padding(start = 24.dp, top = 8.dp),
            ) {
                Text(stringResource(R.string.action_register))
            }
            // TODO: Collect the GuardService state and dispatch ARM/DISARM to it.
            DeviceScreen(
                state = GuardUiState(),
                onArm = {},
                onDisarm = {},
                modifier = Modifier.weight(1f),
            )
        }
        Screen.QR -> QrScanScreen(
            onResult = {},
            modifier = modifier,
            onManualRegister = { screen = Screen.SCAN },
            onBack = ::goBack,
        )
        Screen.SCAN -> ScanScreen(
            onSelect = {},
            modifier = modifier,
            onBack = ::goBack,
        )
    }
}
