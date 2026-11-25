package com.example.lidarscanner

import android.Manifest
import android.app.Activity
import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.wifi.WifiManager
import android.os.BatteryManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.util.Log
import android.view.WindowManager
import com.google.ar.core.ArCoreApk
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry
import java.util.concurrent.Executors

/**
 * Bridges Flutter and the native scanner core:
 *  - MethodChannel "lidarscanner/core": session control, viewport (SurfaceProducer texture), gestures,
 *    phone motion for the simulator (ARCore with IMU fallback) and the camera thumbnail.
 *  - EventChannels "lidarscanner/stats" (4 Hz), "lidarscanner/minimap" (2 Hz), "lidarscanner/revolution" (10 Hz)
 *    and "lidarscanner/layout" (structured model JSON, only when it changed).
 */
class ScannerPlugin(private val activity: Activity, engine: FlutterEngine) : MethodChannel.MethodCallHandler {
    private val textures: TextureRegistry = engine.renderer
    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private val motionWorker = Executors.newSingleThreadExecutor()
    private var producer: TextureRegistry.SurfaceProducer? = null
    private var previewProducer: TextureRegistry.SurfaceProducer? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    @Volatile private var motion: MotionSource? = null
    private var permissionCallback: ((Boolean) -> Unit)? = null

    init {
        val messenger = engine.dartExecutor.binaryMessenger
        MethodChannel(messenger, "lidarscanner/core").setMethodCallHandler(this)
        EventChannel(messenger, "lidarscanner/stats").setStreamHandler(PollingStream(250) { statsEvent() })
        EventChannel(messenger, "lidarscanner/minimap").setStreamHandler(PollingStream(500) { NativeBridge.nativeMinimap() })
        EventChannel(messenger, "lidarscanner/revolution").setStreamHandler(PollingStream(100) { NativeBridge.nativeRevolution() })
        var layoutVersion = -1L
        EventChannel(messenger, "lidarscanner/layout").setStreamHandler(
            PollingStream(400, onListen = { layoutVersion = -1L }) {
                val v = NativeBridge.nativeLayoutVersion()
                if (v == layoutVersion) {
                    null
                } else {
                    layoutVersion = v
                    NativeBridge.nativeLayout().ifEmpty { null }
                }
            },
        )
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "deviceInfo" -> result.success(deviceInfo())
            "start" -> {
                val config = call.argument<String>("config") ?: ""
                val udp = config.lineSequence().any { it == "source=udp" || it == "transport=loopback" }
                worker.execute {
                    if (udp) acquireMulticast()
                    val ok = NativeBridge.nativeStart(config)
                    main.post {
                        if (ok) activity.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        result.success(ok)
                    }
                }
            }
            "stop" -> worker.execute {
                val json = NativeBridge.nativeStop()
                releaseMulticast()
                main.post {
                    stopMotion()
                    activity.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    result.success(json)
                }
            }
            "loadCloud" -> {
                val dir = call.argument<String>("dir") ?: ""
                worker.execute {
                    val json = NativeBridge.nativeLoadCloud(dir)
                    main.post { result.success(json) }
                }
            }
            "computeLayout" -> worker.execute {
                val json = NativeBridge.nativeComputeLayout()
                main.post { result.success(json) }
            }
            "isRunning" -> result.success(NativeBridge.nativeIsRunning())
            "setPaused" -> {
                NativeBridge.nativeSetPaused(call.argument<Boolean>("paused") ?: false)
                result.success(null)
            }
            "startMotion" -> startMotion(result)
            "stopMotion" -> {
                stopMotion()
                result.success(null)
            }
            "createCameraPreview" -> result.success(createCameraPreview(call.int("width"), call.int("height")))
            "disposeCameraPreview" -> {
                disposeCameraPreview()
                result.success(null)
            }
            "stationToggle" -> {
                NativeBridge.nativeStationToggle()
                result.success(null)
            }
            "createViewport" -> result.success(createViewport(call.int("width"), call.int("height")))
            "resizeViewport" -> {
                resizeViewport(call.int("width"), call.int("height"))
                result.success(null)
            }
            "disposeViewport" -> {
                disposeViewport()
                result.success(null)
            }
            "setView" -> {
                NativeBridge.nativeSetView(
                    call.int("camera"), call.int("color"), call.float("pointSize"),
                    call.argument<Boolean>("dark") ?: true,
                    call.argument<Boolean>("trajectory") ?: true,
                    call.argument<Boolean>("grid") ?: true,
                    call.float("ageWindow"), call.int("fps"),
                    call.argument<Boolean>("clipCeiling") ?: false,
                )
                result.success(null)
            }
            "orbit" -> {
                NativeBridge.nativeOrbit(call.float("dx"), call.float("dy"))
                result.success(null)
            }
            "pan" -> {
                NativeBridge.nativePan(call.float("dx"), call.float("dy"))
                result.success(null)
            }
            "zoom" -> {
                NativeBridge.nativeZoom(call.float("scale"))
                result.success(null)
            }
            "resetView" -> {
                NativeBridge.nativeResetView()
                result.success(null)
            }
            else -> result.notImplemented()
        }
    }

    private fun createViewport(width: Int, height: Int): Long {
        disposeViewport()
        val p = textures.createSurfaceProducer()
        p.setSize(width.coerceAtLeast(1), height.coerceAtLeast(1))
        p.setCallback(object : TextureRegistry.SurfaceProducer.Callback {
            override fun onSurfaceAvailable() {
                NativeBridge.nativeSetSurface(p.surface)
            }

            override fun onSurfaceCleanup() {
                NativeBridge.nativeSetSurface(null)
            }
        })
        producer = p
        NativeBridge.nativeSetSurface(p.surface)
        return p.id()
    }

    private fun resizeViewport(width: Int, height: Int) {
        val p = producer ?: return
        if (p.width == width && p.height == height) return
        p.setSize(width.coerceAtLeast(1), height.coerceAtLeast(1))
        NativeBridge.nativeSetSurface(p.surface)
    }

    private fun disposeViewport() {
        val p = producer ?: return
        NativeBridge.nativeSetSurface(null)
        p.release()
        producer = null
    }

    // --- Phone motion for the simulator ---------------------------------------------------

    private var motionGeneration = 0
    private var activityPaused = false

    /** Starts ARCore (or the IMU fallback). Result: {"source": "arcore"|"imu"|"none", "fallbackReason": String?}. */
    private fun startMotion(result: MethodChannel.Result) {
        stopMotion()
        val generation = motionGeneration
        checkArCore(10) { arcore ->
            if (generation != motionGeneration) {
                result.success(mapOf("source" to "none"))
                return@checkArCore
            }
            when (arcore) {
                "ready" -> withCameraPermission { granted ->
                    if (granted) startArCore(result, generation) else startImu(result, "camera_denied")
                }
                "install" -> startImu(result, "arcore_install")
                else -> startImu(result, "arcore_unsupported")
            }
        }
    }

    private fun checkArCore(retries: Int, done: (String) -> Unit) {
        val apk = ArCoreApk.getInstance()
        val availability = try {
            apk.checkAvailability(activity)
        } catch (e: Exception) {
            ArCoreApk.Availability.UNKNOWN_ERROR
        }
        if (availability.isTransient && retries > 0) {
            main.postDelayed({ checkArCore(retries - 1, done) }, 200)
            return
        }
        when (availability) {
            ArCoreApk.Availability.SUPPORTED_INSTALLED -> done("ready")
            ArCoreApk.Availability.SUPPORTED_APK_TOO_OLD, ArCoreApk.Availability.SUPPORTED_NOT_INSTALLED -> {
                // Opens the Play Store flow for "Google Play Services for AR"; this session uses the IMU.
                val status = try {
                    apk.requestInstall(activity, true)
                } catch (e: Exception) {
                    null
                }
                done(
                    when (status) {
                        ArCoreApk.InstallStatus.INSTALLED -> "ready"
                        ArCoreApk.InstallStatus.INSTALL_REQUESTED -> "install"
                        else -> "unsupported"
                    },
                )
            }
            else -> done("unsupported")
        }
    }

    private fun withCameraPermission(done: (Boolean) -> Unit) {
        if (activity.checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            done(true)
            return
        }
        permissionCallback?.invoke(false)
        permissionCallback = done
        activity.requestPermissions(arrayOf(Manifest.permission.CAMERA), CAMERA_REQUEST)
    }

    fun onRequestPermissionsResult(requestCode: Int, grantResults: IntArray) {
        if (requestCode != CAMERA_REQUEST) return
        val cb = permissionCallback ?: return
        permissionCallback = null
        cb(grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)
    }

    private fun startArCore(result: MethodChannel.Result, generation: Int) {
        motionWorker.execute {
            val source = ArCoreMotionSource(activity)
            val error = try {
                source.start()
                null
            } catch (e: Exception) {
                Log.w(TAG, "ARCore unavailable, falling back to IMU", e)
                e
            }
            main.post {
                when {
                    error != null -> startImu(result, "arcore_error")
                    generation != motionGeneration -> {
                        motionWorker.execute { source.stop() }
                        result.success(mapOf("source" to "none"))
                    }
                    else -> {
                        if (activityPaused) source.onPause()
                        motion = source
                        result.success(mapOf("source" to "arcore"))
                    }
                }
            }
        }
    }

    private fun startImu(result: MethodChannel.Result, reason: String) {
        val source = ImuMotionSource(activity)
        if (!source.available) {
            result.success(mapOf("source" to "none", "fallbackReason" to reason))
            return
        }
        source.start()
        motion = source
        result.success(mapOf("source" to "imu", "fallbackReason" to reason))
    }

    private fun stopMotion() {
        ++motionGeneration
        disposeCameraPreview()
        val m = motion ?: return
        motion = null
        motionWorker.execute { m.stop() }
    }

    private fun createCameraPreview(width: Int, height: Int): Long? {
        val m = motion ?: return null
        if (m.kind != "arcore") return null
        disposeCameraPreview()
        val p = textures.createSurfaceProducer()
        p.setSize(width.coerceAtLeast(1), height.coerceAtLeast(1))
        p.setCallback(object : TextureRegistry.SurfaceProducer.Callback {
            override fun onSurfaceAvailable() {
                m.setPreviewSurface(p.surface, p.width, p.height)
            }

            override fun onSurfaceCleanup() {
                m.setPreviewSurface(null, 0, 0)
            }
        })
        previewProducer = p
        m.setPreviewSurface(p.surface, p.width, p.height)
        return p.id()
    }

    private fun disposeCameraPreview() {
        val p = previewProducer ?: return
        motion?.setPreviewSurface(null, 0, 0)
        p.release()
        previewProducer = null
    }

    fun onPause() {
        activityPaused = true
        motion?.onPause()
    }

    fun onResume() {
        activityPaused = false
        motion?.onResume()
    }

    private fun deviceInfo(): Map<String, Any> {
        val am = activity.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val mem = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        val ramMb = mem.totalMem / (1024 * 1024)
        val budget = when {
            ramMb >= 7000 -> 3_000_000
            ramMb >= 5000 -> 2_000_000
            else -> 1_000_000
        }
        return mapOf(
            "model" to "${Build.MANUFACTURER} ${Build.MODEL}",
            "sdkInt" to Build.VERSION.SDK_INT,
            "totalRamMb" to ramMb,
            "lowEnd" to (ramMb < 5000 || am.isLowRamDevice),
            "recommendedBudget" to budget,
            "glEsVersion" to am.deviceConfigurationInfo.glEsVersion,
            "cpuCores" to Runtime.getRuntime().availableProcessors(),
        )
    }

    private fun statsEvent(): Map<String, Any?> {
        val battery = activity.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = battery?.let {
            val l = it.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
            val s = it.getIntExtra(BatteryManager.EXTRA_SCALE, 100)
            if (l >= 0 && s > 0) l * 100 / s else -1
        } ?: -1
        val temp = (battery?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0) / 10.0
        val status = battery?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val thermal = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            (activity.getSystemService(Context.POWER_SERVICE) as PowerManager).currentThermalStatus
        } else {
            -1
        }
        return mapOf(
            "core" to NativeBridge.nativeStats(),
            "batteryLevel" to level,
            "batteryTempC" to temp,
            "charging" to (status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL),
            "thermalStatus" to thermal,
            "motionSource" to motion?.kind,
            "motionFailure" to motion?.trackingFailure,
        )
    }

    private fun acquireMulticast() {
        if (multicastLock != null) return
        val wifi = activity.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wifi.createMulticastLock("lidarscanner").apply {
            setReferenceCounted(false)
            acquire()
        }
    }

    private fun releaseMulticast() {
        multicastLock?.release()
        multicastLock = null
    }

    private fun MethodCall.float(key: String): Float = (argument<Number>(key) ?: 0).toFloat()
    private fun MethodCall.int(key: String): Int = (argument<Number>(key) ?: 0).toInt()

    private companion object {
        const val TAG = "ScannerPlugin"
        const val CAMERA_REQUEST = 4210
    }

    /** Polls |source| on the main thread while Dart listens. Null values are skipped. */
    private inner class PollingStream(
        private val periodMs: Long,
        private val onListen: () -> Unit = {},
        private val source: () -> Any?,
    ) : EventChannel.StreamHandler {
        private var sink: EventChannel.EventSink? = null
        private val tick = object : Runnable {
            override fun run() {
                val s = sink ?: return
                source()?.let { s.success(it) }
                main.postDelayed(this, periodMs)
            }
        }

        override fun onListen(arguments: Any?, events: EventChannel.EventSink) {
            sink = events
            onListen()
            main.removeCallbacks(tick)
            main.post(tick)
        }

        override fun onCancel(arguments: Any?) {
            sink = null
            main.removeCallbacks(tick)
        }
    }
}
