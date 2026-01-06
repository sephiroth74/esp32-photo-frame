import Cocoa
import FlutterMacOS

@main
class AppDelegate: FlutterAppDelegate {
  override func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
    return true
  }

  override func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
    return true
  }

  override func applicationDidFinishLaunching(_ notification: Notification) {
    let controller = mainFlutterWindow?.contentViewController as! FlutterViewController
    let fontChannel = FlutterMethodChannel(name: "com.photoframe/fonts",
                                           binaryMessenger: controller.engine.binaryMessenger)

    fontChannel.setMethodCallHandler { (call, result) in
      if call.method == "getSystemFonts" {
        let fontFamilies = NSFontManager.shared.availableFontFamilies
        let sortedFonts = fontFamilies.filter { !$0.hasPrefix(".") }.sorted()
        result(sortedFonts)
      } else {
        result(FlutterMethodNotImplemented)
      }
    }
  }
}
