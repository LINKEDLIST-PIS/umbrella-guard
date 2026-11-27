package com.umbrellaguard.app.notify

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.core.app.NotificationCompat
import com.umbrellaguard.app.R

/** 알림 채널 두 개 (APP_SPEC 5.4) */
object Notifications {
    const val CH_STATUS = "guard_status"
    const val CH_ALARM = "guard_alarm"
    const val ID_STATUS = 1
    const val ID_ALARM = 2

    fun createChannels(context: Context) {
        val nm = context.getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(
            NotificationChannel(
                CH_STATUS,
                context.getString(R.string.channel_status),
                NotificationManager.IMPORTANCE_LOW,
            ),
        )
        nm.createNotificationChannel(
            NotificationChannel(
                CH_ALARM,
                context.getString(R.string.channel_alarm),
                NotificationManager.IMPORTANCE_HIGH,
            ).apply {
                lockscreenVisibility = Notification.VISIBILITY_PUBLIC
                enableVibration(true)
            },
        )
    }

    /** Foreground Service 상태 알림 (예: "우산 보관 중") */
    fun status(context: Context, text: String): Notification =
        NotificationCompat.Builder(context, CH_STATUS)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle(context.getString(R.string.app_name))
            .setContentText(text)
            .setOngoing(true)
            .build()

    // TODO(F-5, F-6): 경보 알림(해제 액션 포함), 연결 끊김 알림, 재시작 안내 알림(F-8)
}
