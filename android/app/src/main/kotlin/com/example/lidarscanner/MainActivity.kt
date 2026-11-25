package com.example.lidarscanner

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine

class MainActivity : FlutterActivity() {
    private var plugin: ScannerPlugin? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        plugin = ScannerPlugin(this, flutterEngine)
    }

    override fun onPause() {
        plugin?.onPause()
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        plugin?.onResume()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        plugin?.onRequestPermissionsResult(requestCode, grantResults)
    }
}
