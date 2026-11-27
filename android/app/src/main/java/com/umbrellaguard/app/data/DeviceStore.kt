package com.umbrellaguard.app.data

import android.content.Context
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "device")

/**
 * 등록된 기기 MAC 주소 저장 (APP_SPEC F-1). PIN 은 저장하지 않는다.
 */
class DeviceStore(private val context: Context) {
    private val keyAddress = stringPreferencesKey("address")

    val savedAddress: Flow<String?> = context.dataStore.data.map { it[keyAddress] }

    suspend fun save(address: String) {
        context.dataStore.edit { it[keyAddress] = address }
    }

    suspend fun clear() {
        context.dataStore.edit { it.remove(keyAddress) }
    }
}
