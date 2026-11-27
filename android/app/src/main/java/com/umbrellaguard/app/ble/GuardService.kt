package com.umbrellaguard.app.ble

import android.Manifest
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.umbrellaguard.app.R
import com.umbrellaguard.app.notify.Notifications

/**
 * 보관 모드 동안 BLE 연결을 소유하는 Foreground Service (APP_SPEC 5.4).
 * Activity 가 연결을 들고 있으면 화면이 꺼질 때 끊기므로 반드시 여기서 관리한다.
 *
 * 시작 조건: Android 12 이상에서는 BLUETOOTH_CONNECT 권한이 허용된 뒤에만 시작한다.
 * Android 14 이상에서 권한 없이 connectedDevice 타입으로 startForeground 하면 SecurityException.
 * 호출하는 쪽(Activity)에서 권한을 먼저 받고 startForegroundService 할 것.
 *
 * TODO:
 *  - BleClient 를 소유하고 state 를 UI 에 노출 (바인딩 또는 싱글턴 저장소)
 *  - 연결/상태에 따라 상태 알림 문구 갱신 (연결 확인 중 -> 보관 중)
 *  - ALARM 수신 시 경보 알림 1 회 + "해제" 액션 -> DISARM 전송 (F-5)
 *  - DISARMED 가 되면 stopSelf()
 *  - 재연결(F-6/F-7)과 재시작 안내(F-8)를 구현한 뒤 START_STICKY 로 바꿀지 결정
 */
class GuardService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (!hasConnectPermission()) {
            Log.w(TAG, "BLUETOOTH_CONNECT 권한 없음 - 서비스를 시작하지 않음")
            stopSelf(startId)
            return START_NOT_STICKY
        }

        // 실제 연결과 STATE 를 확인하기 전이므로 "보관 중" 이 아니라 "연결 확인 중" 으로 시작한다.
        val notification = Notifications.status(this, getString(R.string.status_connecting))
        val type = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
        } else {
            0
        }
        ServiceCompat.startForeground(this, Notifications.ID_STATUS, notification, type)

        // START_STICKY 로 재시작되면 연결이 없는데도 알림만 떠 있게 된다.
        // 재연결과 F-8(재시작 안내)을 구현하기 전까지는 재시작하지 않는다.
        return START_NOT_STICKY
    }

    private fun hasConnectPermission(): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) ==
            PackageManager.PERMISSION_GRANTED

    private companion object {
        const val TAG = "GuardService"
    }
}
