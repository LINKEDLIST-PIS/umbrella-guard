package com.umbrellaguard.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import com.umbrellaguard.app.ble.GuardUiState
import com.umbrellaguard.app.ui.DeviceScreen

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        // targetSdk 35: Android 15 기기는 edge-to-edge 가 강제된다.
        // 모든 버전에서 같은 화면이 되도록 직접 켜고, 내용은 시스템 바 안쪽(safeDrawing)에 그린다.
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(Modifier.fillMaxSize()) {
                    // TODO: 저장된 기기가 없으면 QrScanScreen, 있으면 GuardService 상태를 구독해 DeviceScreen
                    DeviceScreen(
                        state = GuardUiState(),
                        onArm = {},
                        onDisarm = {},
                        modifier = Modifier.safeDrawingPadding(),
                    )
                }
            }
        }
    }
}
