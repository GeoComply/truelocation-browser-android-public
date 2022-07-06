package org.mozilla.fenix.utils

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build

class Util {
    companion object {
        fun wasRequiredPermissionsGranted(
            contextWrapper: Context,
            permissions: Array<String>
        ): Boolean {
            if (hasMarshmallow()) {
                for (permission in permissions) {
                    if (contextWrapper.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                        return true
                    }
                }
            }
            return false
        }

        //----------------------------------------------------------------------------------------------
        fun hasMarshmallow(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
        }

        //----------------------------------------------------------------------------------------------
        fun hasOreo(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
        }

        //----------------------------------------------------------------------------------------------
        fun hasPie(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
        }

        //----------------------------------------------------------------------------------------------
        fun hasQ(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
        }

        //----------------------------------------------------------------------------------------------
        fun hasR(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        }

        //----------------------------------------------------------------------------------------------
        fun hasS(): Boolean {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
        }

        val REQUIRED_PERMISSIONS = arrayOf<String>(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )

        const val SHARE_PREFERENCE_KEY = "com.geocomply.tlb.sharepreference"
        private const val SHOW_WELCOME_SCREEN = "show_welcome_screen"
        fun showWelcomeScreen(context: Context) : Boolean {
            val sharePreference = context.getSharedPreferences(SHARE_PREFERENCE_KEY, Context.MODE_PRIVATE)
            //Default will return true if SHOW_WELCOME_SCREEN have not set
            return sharePreference.getBoolean(SHOW_WELCOME_SCREEN, true)
        }

        fun finishWelcomeScreen(context: Context) {
            val sharePreference = context.getSharedPreferences(SHARE_PREFERENCE_KEY, Context.MODE_PRIVATE)
            sharePreference.edit().putBoolean(SHOW_WELCOME_SCREEN, false).apply()
        }
    }
}