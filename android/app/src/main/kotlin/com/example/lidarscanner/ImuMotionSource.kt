package com.example.lidarscanner

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Handler
import android.os.HandlerThread
import kotlin.math.min
import kotlin.math.sqrt

/**
 * Fallback for phones without ARCore: orientation from the game rotation vector (gyroscope +
 * accelerometer) and position from pedestrian dead reckoning (each detected step advances a
 * fixed stride in the direction the phone is pointing). Height is not observable and stays at 0.
 */
class ImuMotionSource(context: Context) : MotionSource, SensorEventListener {
    override val kind = "imu"

    private val sensors = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val rotationSensor: Sensor? =
        sensors.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR) ?: sensors.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)
    private val accelSensor: Sensor? = sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private var thread: HandlerThread? = null

    val available: Boolean get() = rotationSensor != null

    // Orientation, Android world frame (X east, Y north, Z up) <- device.
    private val qa = FloatArray(4)
    private var haveRotation = false

    // Dead reckoning (core frame, Y up).
    private var px = 0f
    private var pz = 0f
    private var pending = 0f
    private var lastMoveNs = 0L

    // Step detector on the low-passed vertical acceleration.
    private var vertical = 0f
    private var armed = false
    private var lastStepNs = 0L

    override fun start() {
        val r = rotationSensor ?: return
        val t = HandlerThread("imu-motion").also { it.start() }
        thread = t
        val handler = Handler(t.looper)
        sensors.registerListener(this, r, SAMPLING_US, handler)
        accelSensor?.let { sensors.registerListener(this, it, SAMPLING_US, handler) }
    }

    override fun stop() {
        sensors.unregisterListener(this)
        thread?.quitSafely()
        thread = null
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onSensorChanged(e: SensorEvent) {
        when (e.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> onAccel(e)
            else -> onRotation(e)
        }
    }

    private fun onAccel(e: SensorEvent) {
        if (!haveRotation) return
        val w = qa[0]
        val x = qa[1]
        val y = qa[2]
        val z = qa[3]
        // Z row of the rotation matrix: vertical component of the specific force in the world.
        val up = 2 * (x * z - w * y) * e.values[0] + 2 * (y * z + w * x) * e.values[1] +
            (1 - 2 * (x * x + y * y)) * e.values[2] - SensorManager.GRAVITY_EARTH
        vertical += LOW_PASS * (up - vertical)
        if (vertical < STEP_REARM) armed = true
        if (armed && vertical > STEP_PEAK && e.timestamp - lastStepNs > MIN_STEP_NS) {
            armed = false
            lastStepNs = e.timestamp
            pending += STRIDE_M
        }
    }

    private fun onRotation(e: SensorEvent) {
        SensorManager.getQuaternionFromVector(qa, e.values)
        haveRotation = true
        val c = 0.70710677f
        // Rotate the Android world (Z up) into the core world (Y up): (x, y, z) -> (x, z, -y).
        val w = c * (qa[0] + qa[1])
        val x = c * (qa[1] - qa[0])
        val y = c * (qa[2] + qa[3])
        val z = c * (qa[3] - qa[2])

        val dt = if (lastMoveNs == 0L) 0f else (e.timestamp - lastMoveNs) * 1e-9f
        lastMoveNs = e.timestamp
        if (pending > 1e-4f && dt > 0f) {
            val step = pending * min(1f, dt / STEP_SMOOTH_S)
            pending -= step
            var f = Quats.rotate(w, x, y, z, 0f, 0f, -1f)
            // Phone held flat: walk towards the top edge instead.
            if (f[0] * f[0] + f[2] * f[2] < 0.04f) {
                val sign = if (f[1] < 0) 1f else -1f
                f = Quats.rotate(w, x, y, z, 0f, 1f, 0f).map { it * sign }.toFloatArray()
            }
            val n = sqrt(f[0] * f[0] + f[2] * f[2])
            if (n > 1e-3f) {
                px += f[0] / n * step
                pz += f[2] / n * step
            }
        }
        NativeBridge.nativePushMotion(MotionTime.toBoottime(e.timestamp), px, 0f, pz, x, y, z, w, true)
    }

    private companion object {
        const val SAMPLING_US = 10_000
        const val LOW_PASS = 0.25f
        const val STEP_PEAK = 0.9f      // m/s^2 above gravity
        const val STEP_REARM = -0.4f
        const val MIN_STEP_NS = 280_000_000L
        const val STRIDE_M = 0.65f
        const val STEP_SMOOTH_S = 0.18f
    }
}
