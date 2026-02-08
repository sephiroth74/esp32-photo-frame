package it.sephiroth.photoframe.photoframe

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
	private var connectivityManager: ConnectivityManager? = null
	private var wifiCallback: ConnectivityManager.NetworkCallback? = null

	override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
		super.configureFlutterEngine(flutterEngine)

		MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "photoframe/wifi")
			.setMethodCallHandler { call, result ->
				when (call.method) {
					"bindToWifi" -> {
						bindToWifi()
						result.success(true)
					}
					"clearWifiBinding" -> {
						clearWifiBinding()
						result.success(true)
					}
					else -> result.notImplemented()
				}
			}
	}

	private fun bindToWifi() {
		val cm = connectivityManager
			?: (getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager).also {
				connectivityManager = it
			}

		val active = cm.activeNetwork
		val caps = active?.let { cm.getNetworkCapabilities(it) }
		if (active != null && caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true) {
			cm.bindProcessToNetwork(active)
			android.util.Log.i("PhotoFrame", "[WiFi Binding] SUCCESS: Using active WiFi network")
			return
		}

		android.util.Log.w("PhotoFrame", "[WiFi Binding] No active WiFi, requesting network with 15s timeout...")
		if (wifiCallback != null) {
			android.util.Log.w("PhotoFrame", "[WiFi Binding] Callback already active, skipping")
			return
		}

		val request = NetworkRequest.Builder()
			.addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
			.build()

		val callback = object : ConnectivityManager.NetworkCallback() {
			override fun onAvailable(network: Network) {
				android.util.Log.i("PhotoFrame", "[WiFi Binding] WiFi network available, binding process...")
				cm.bindProcessToNetwork(network)
			}

			override fun onUnavailable() {
				android.util.Log.e("PhotoFrame", "[WiFi Binding] WiFi network request timed out or unavailable")
				clearWifiBinding()
			}

			override fun onLost(network: Network) {
				android.util.Log.w("PhotoFrame", "[WiFi Binding] WiFi network lost")
				clearWifiBinding()
			}
		}

		wifiCallback = callback
		// Request network with 15 second timeout
		cm.requestNetwork(request, callback, 15000)
		android.util.Log.i("PhotoFrame", "[WiFi Binding] Network request submitted")
	}

	private fun clearWifiBinding() {
		val cm = connectivityManager
			?: (getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager).also {
				connectivityManager = it
			}

		cm.bindProcessToNetwork(null)
		wifiCallback?.let {
			try {
				cm.unregisterNetworkCallback(it)
			} catch (_: Exception) {
			}
		}
		wifiCallback = null
	}
}
