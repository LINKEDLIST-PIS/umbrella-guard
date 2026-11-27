package com.umbrellaguard.app

import android.app.Application
import com.umbrellaguard.app.notify.Notifications

class UmbrellaGuardApp : Application() {
    override fun onCreate() {
        super.onCreate()
        Notifications.createChannels(this)
    }
}
