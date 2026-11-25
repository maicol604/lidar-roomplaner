package com.example.lidarscanner

import android.view.Surface

/** JNI entry points of the native scanner core (android/app/src/main/cpp). */
object NativeBridge {
    init {
        System.loadLibrary("lidarcore")
    }

    external fun nativeStart(config: String): Boolean
    external fun nativeStop(): String
    external fun nativeLoadCloud(dir: String): String
    external fun nativeIsRunning(): Boolean
    external fun nativeSetPaused(paused: Boolean)
    /** Real phone pose (tracker frame, Y up) that drives the simulated rig. */
    external fun nativePushMotion(
        tNs: Long,
        px: Float,
        py: Float,
        pz: Float,
        qx: Float,
        qy: Float,
        qz: Float,
        qw: Float,
        tracking: Boolean,
    )
    external fun nativeStationToggle()
    external fun nativeStats(): String
    external fun nativeMinimap(): ByteArray?
    external fun nativeRevolution(): FloatArray
    /** Changes every time the layout (walls, openings, furniture, rooms) is refreshed. */
    external fun nativeLayoutVersion(): Long
    /** Latest layout JSON (empty before the first refresh). */
    external fun nativeLayout(): String
    /** Computes the layout of the loaded map now (saved sessions); returns its JSON. */
    external fun nativeComputeLayout(): String
    external fun nativeSetSurface(surface: Surface?)
    external fun nativeSetView(
        camera: Int,
        color: Int,
        pointSize: Float,
        dark: Boolean,
        trajectory: Boolean,
        grid: Boolean,
        ageWindow: Float,
        fps: Int,
        clipCeiling: Boolean,
    )
    external fun nativeOrbit(dx: Float, dy: Float)
    external fun nativePan(dx: Float, dy: Float)
    external fun nativeZoom(scale: Float)
    external fun nativeResetView()
}
