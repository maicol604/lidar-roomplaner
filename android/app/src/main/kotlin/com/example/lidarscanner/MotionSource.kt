package com.example.lidarscanner

import android.os.SystemClock
import android.view.Surface
import kotlin.math.abs

/**
 * Real phone motion that drives the simulated LiDAR rig. Poses are pushed to the native core
 * (NativeBridge.nativePushMotion) in a gravity-aligned frame with Y up, using the device frame
 * of the core: X right, Y up (top edge in portrait), -Z out of the back camera.
 */
interface MotionSource {
    /** "arcore" or "imu". */
    val kind: String

    /** Last reason why tracking is limited (e.g. "insufficient_light"), or null. */
    val trackingFailure: String? get() = null

    fun start()
    fun stop()
    fun onPause() {}
    fun onResume() {}
    fun setPreviewSurface(surface: Surface?, width: Int, height: Int) {}
}

internal object MotionTime {
    /** Maps a sensor / camera timestamp onto CLOCK_BOOTTIME, the native core time base. */
    fun toBoottime(ts: Long): Long {
        val boot = SystemClock.elapsedRealtimeNanos()
        if (abs(boot - ts) < 1_000_000_000L) return ts
        val mono = System.nanoTime()
        if (abs(mono - ts) < 1_000_000_000L) return ts + (boot - mono)
        return boot
    }
}

/** Quaternion helpers (w, x, y, z). */
internal object Quats {
    fun rotate(w: Float, x: Float, y: Float, z: Float, vx: Float, vy: Float, vz: Float): FloatArray {
        // v' = v + 2w(u x v) + 2u x (u x v)
        val tx = 2 * (y * vz - z * vy)
        val ty = 2 * (z * vx - x * vz)
        val tz = 2 * (x * vy - y * vx)
        return floatArrayOf(
            vx + w * tx + (y * tz - z * ty),
            vy + w * ty + (z * tx - x * tz),
            vz + w * tz + (x * ty - y * tx),
        )
    }
}
