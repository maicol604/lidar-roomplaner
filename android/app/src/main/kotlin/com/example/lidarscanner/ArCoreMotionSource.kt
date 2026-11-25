package com.example.lidarscanner

import android.app.Activity
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.opengl.GLES11Ext
import android.opengl.GLES20
import android.util.Log
import android.view.Surface
import com.google.ar.core.Config
import com.google.ar.core.Coordinates2d
import com.google.ar.core.Frame
import com.google.ar.core.Session
import com.google.ar.core.TrackingFailureReason
import com.google.ar.core.TrackingState
import com.google.ar.core.exceptions.CameraNotAvailableException
import com.google.ar.core.exceptions.SessionPausedException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer

/**
 * 6-DoF phone motion from ARCore (camera + IMU visual-inertial odometry). Runs the session on
 * its own thread with an offscreen EGL context (ARCore needs one for the camera texture) and,
 * when a preview surface is attached, draws the camera image into it as a thumbnail.
 *
 * Poses are Camera.getDisplayOrientedPose() for a portrait display, which matches the device
 * frame of the native core (X right, Y up, -Z out of the back camera). The ARCore world frame
 * is gravity aligned with Y up.
 */
class ArCoreMotionSource(private val activity: Activity) : MotionSource {
    override val kind = "arcore"

    @Volatile override var trackingFailure: String? = null
        private set

    @Volatile private var running = false
    @Volatile private var paused = false
    private var session: Session? = null
    private var thread: Thread? = null

    private val surfaceLock = Any()
    private var nextSurface: Surface? = null
    private var nextWidth = 0
    private var nextHeight = 0
    private var surfaceChanged = false

    /** Creates and configures the session; throws ARCore UnavailableException subclasses. */
    override fun start() {
        val s = Session(activity)
        val cfg = Config(s).apply {
            updateMode = Config.UpdateMode.BLOCKING
            focusMode = Config.FocusMode.AUTO
            planeFindingMode = Config.PlaneFindingMode.DISABLED
            lightEstimationMode = Config.LightEstimationMode.DISABLED
            depthMode = Config.DepthMode.DISABLED
            instantPlacementMode = Config.InstantPlacementMode.DISABLED
        }
        s.configure(cfg)
        session = s
        running = true
        thread = Thread({ loop(s) }, "arcore-motion").also { it.start() }
    }

    override fun stop() {
        running = false
        thread?.join(3000)
        thread = null
        session = null
    }

    override fun onPause() {
        paused = true
    }

    override fun onResume() {
        paused = false
    }

    override fun setPreviewSurface(surface: Surface?, width: Int, height: Int) {
        synchronized(surfaceLock) {
            nextSurface = surface
            nextWidth = width
            nextHeight = height
            surfaceChanged = true
        }
    }

    private fun loop(s: Session) {
        val gl = Gl()
        var resumed = false
        try {
            gl.init()
            s.setCameraTextureName(gl.cameraTexture)
            s.setDisplayGeometry(Surface.ROTATION_0, DEFAULT_W, DEFAULT_H)
            while (running) {
                if (paused) {
                    if (resumed) {
                        s.pause()
                        resumed = false
                    }
                    pushLost()
                    Thread.sleep(100)
                    continue
                }
                if (!resumed) {
                    try {
                        s.resume()
                        resumed = true
                    } catch (e: CameraNotAvailableException) {
                        trackingFailure = "camera_unavailable"
                        pushLost()
                        Thread.sleep(500)
                        continue
                    }
                }
                applySurfaceChange(gl, s)
                val frame = try {
                    s.update()
                } catch (e: CameraNotAvailableException) {
                    s.pause()
                    resumed = false
                    trackingFailure = "camera_unavailable"
                    pushLost()
                    Thread.sleep(300)
                    continue
                } catch (e: SessionPausedException) {
                    resumed = false
                    continue
                }
                if (frame.timestamp == 0L) continue
                publish(frame)
                if (gl.hasWindow) gl.drawPreview(frame)
            }
        } catch (e: InterruptedException) {
            // Stopping.
        } catch (e: Exception) {
            Log.e(TAG, "ARCore loop failed", e)
            trackingFailure = "bad_state"
            pushLost()
        } finally {
            try {
                if (resumed) s.pause()
                s.close()
            } catch (e: Exception) {
                Log.w(TAG, "ARCore close failed", e)
            }
            gl.release()
        }
    }

    private fun publish(frame: Frame) {
        val cam = frame.camera
        val tracking = cam.trackingState == TrackingState.TRACKING
        trackingFailure = if (tracking) null else when (cam.trackingFailureReason) {
            TrackingFailureReason.INSUFFICIENT_LIGHT -> "insufficient_light"
            TrackingFailureReason.EXCESSIVE_MOTION -> "excessive_motion"
            TrackingFailureReason.INSUFFICIENT_FEATURES -> "insufficient_features"
            TrackingFailureReason.CAMERA_UNAVAILABLE -> "camera_unavailable"
            TrackingFailureReason.BAD_STATE -> "bad_state"
            else -> "initializing"
        }
        val p = cam.displayOrientedPose
        NativeBridge.nativePushMotion(
            MotionTime.toBoottime(frame.timestamp), p.tx(), p.ty(), p.tz(), p.qx(), p.qy(), p.qz(), p.qw(), tracking,
        )
    }

    private fun pushLost() {
        NativeBridge.nativePushMotion(MotionTime.toBoottime(android.os.SystemClock.elapsedRealtimeNanos()), 0f, 0f, 0f, 0f, 0f, 0f, 1f, false)
    }

    private fun applySurfaceChange(gl: Gl, s: Session) {
        val surface: Surface?
        val w: Int
        val h: Int
        synchronized(surfaceLock) {
            if (!surfaceChanged) return
            surfaceChanged = false
            surface = nextSurface
            w = nextWidth
            h = nextHeight
        }
        try {
            gl.setWindow(surface, w, h)
        } catch (e: Exception) {
            Log.w(TAG, "Camera preview surface unavailable", e)
            gl.setWindow(null, 0, 0)
            return
        }
        // The display geometry defines the crop of the camera image (and the display-oriented
        // pose); keep it portrait so the pose convention does not change.
        if (surface != null && w > 0 && h > 0) s.setDisplayGeometry(Surface.ROTATION_0, w, h)
    }

    /** Offscreen EGL context + optional window surface for the camera thumbnail. */
    private class Gl {
        private var display: EGLDisplay = EGL14.EGL_NO_DISPLAY
        private var context: EGLContext = EGL14.EGL_NO_CONTEXT
        private var config: EGLConfig? = null
        private var pbuffer: EGLSurface = EGL14.EGL_NO_SURFACE
        private var window: EGLSurface = EGL14.EGL_NO_SURFACE
        private var width = 0
        private var height = 0
        var cameraTexture = 0
            private set
        private var program = 0
        private var aPos = 0
        private var aUv = 0
        private val ndc: FloatBuffer = floatBuffer(floatArrayOf(-1f, -1f, 1f, -1f, -1f, 1f, 1f, 1f))
        private val uv: FloatBuffer = floatBuffer(FloatArray(8))
        private var uvValid = false

        val hasWindow: Boolean get() = window != EGL14.EGL_NO_SURFACE

        fun init() {
            display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
            val version = IntArray(2)
            check(EGL14.eglInitialize(display, version, 0, version, 1)) { "eglInitialize" }
            val attrs = intArrayOf(
                EGL14.EGL_RED_SIZE, 8, EGL14.EGL_GREEN_SIZE, 8, EGL14.EGL_BLUE_SIZE, 8, EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT or EGL14.EGL_WINDOW_BIT,
                EGL14.EGL_NONE,
            )
            val configs = arrayOfNulls<EGLConfig>(1)
            val n = IntArray(1)
            check(EGL14.eglChooseConfig(display, attrs, 0, configs, 0, 1, n, 0) && n[0] > 0) { "eglChooseConfig" }
            config = configs[0]
            context = EGL14.eglCreateContext(
                display, config, EGL14.EGL_NO_CONTEXT, intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE), 0,
            )
            pbuffer = EGL14.eglCreatePbufferSurface(display, config, intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE), 0)
            check(EGL14.eglMakeCurrent(display, pbuffer, pbuffer, context)) { "eglMakeCurrent" }

            val tex = IntArray(1)
            GLES20.glGenTextures(1, tex, 0)
            cameraTexture = tex[0]
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, cameraTexture)
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
            program = buildProgram()
            aPos = GLES20.glGetAttribLocation(program, "aPos")
            aUv = GLES20.glGetAttribLocation(program, "aUv")
        }

        fun setWindow(surface: Surface?, w: Int, h: Int) {
            EGL14.eglMakeCurrent(display, pbuffer, pbuffer, context)
            if (window != EGL14.EGL_NO_SURFACE) {
                EGL14.eglDestroySurface(display, window)
                window = EGL14.EGL_NO_SURFACE
            }
            if (surface != null && surface.isValid) {
                window = EGL14.eglCreateWindowSurface(display, config, surface, intArrayOf(EGL14.EGL_NONE), 0)
            }
            width = w
            height = h
            uvValid = false
            if (window != EGL14.EGL_NO_SURFACE) EGL14.eglMakeCurrent(display, window, window, context)
        }

        fun drawPreview(frame: Frame) {
            if (!uvValid || frame.hasDisplayGeometryChanged()) {
                ndc.position(0)
                uv.position(0)
                frame.transformCoordinates2d(
                    Coordinates2d.OPENGL_NORMALIZED_DEVICE_COORDINATES, ndc, Coordinates2d.TEXTURE_NORMALIZED, uv,
                )
                uvValid = true
            }
            GLES20.glViewport(0, 0, width, height)
            GLES20.glDisable(GLES20.GL_DEPTH_TEST)
            GLES20.glUseProgram(program)
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, cameraTexture)
            ndc.position(0)
            uv.position(0)
            GLES20.glVertexAttribPointer(aPos, 2, GLES20.GL_FLOAT, false, 0, ndc)
            GLES20.glVertexAttribPointer(aUv, 2, GLES20.GL_FLOAT, false, 0, uv)
            GLES20.glEnableVertexAttribArray(aPos)
            GLES20.glEnableVertexAttribArray(aUv)
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)
            GLES20.glDisableVertexAttribArray(aPos)
            GLES20.glDisableVertexAttribArray(aUv)
            EGL14.eglSwapBuffers(display, window)
        }

        fun release() {
            if (display == EGL14.EGL_NO_DISPLAY) return
            EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
            if (window != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, window)
            if (pbuffer != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, pbuffer)
            if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display, context)
            EGL14.eglTerminate(display)
            display = EGL14.EGL_NO_DISPLAY
        }

        private fun buildProgram(): Int {
            fun shader(type: Int, src: String): Int {
                val s = GLES20.glCreateShader(type)
                GLES20.glShaderSource(s, src)
                GLES20.glCompileShader(s)
                return s
            }
            val p = GLES20.glCreateProgram()
            GLES20.glAttachShader(p, shader(GLES20.GL_VERTEX_SHADER, VERTEX))
            GLES20.glAttachShader(p, shader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT))
            GLES20.glLinkProgram(p)
            return p
        }

        private fun floatBuffer(v: FloatArray): FloatBuffer =
            ByteBuffer.allocateDirect(v.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().apply {
                put(v)
                position(0)
            }
    }

    private companion object {
        const val TAG = "ArCoreMotion"
        const val DEFAULT_W = 480
        const val DEFAULT_H = 640
        const val VERTEX = """
            attribute vec2 aPos;
            attribute vec2 aUv;
            varying vec2 vUv;
            void main() {
              gl_Position = vec4(aPos, 0.0, 1.0);
              vUv = aUv;
            }
        """
        const val FRAGMENT = """
            #extension GL_OES_EGL_image_external : require
            precision mediump float;
            varying vec2 vUv;
            uniform samplerExternalOES uTex;
            void main() {
              gl_FragColor = texture2D(uTex, vUv);
            }
        """
    }
}
