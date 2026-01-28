//
//  QuickLookDemoApp.swift
//  QuickLookDemo
//
//  App that opens .pfr1 files from Finder and shows them borderless at exact image size.
//

import SwiftUI
internal import AppKit

final class AppDelegate: NSObject, NSApplicationDelegate {
    var onOpenURL: ((URL) -> Void)?

    func application(_ sender: NSApplication, openFile filename: String) -> Bool {
        onOpenURL?(URL(fileURLWithPath: filename))
        return true
    }

    func application(_ application: NSApplication, open urls: [URL]) {
        if let first = urls.first { onOpenURL?(first) }
    }
}

@main
struct QuickLookApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @State private var openedURL: URL? = nil
    @State private var isReady = false
    @State private var currentWindow: NSWindow?
    @State private var imageSize: CGSize? = nil

    var body: some Scene {
        WindowGroup {
            Group {
                if let url = openedURL {
                    // Instantiate the viewer immediately so it can decode and post size
                    PFR1ImageView(url: url, onImageSize: { _ in })
                        .id(url)
                } else {
                    // No file provided: show message
                    Text("Open a .pfr1 file")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .multilineTextAlignment(.center)
                        .padding()
                }
            }
            .background(WindowAccessor { window in
                self.currentWindow = window
            })
            .onOpenURL { url in
                // Start loading new file: hide window and reset readiness
                openedURL = url
                isReady = false
                currentWindow?.orderOut(nil)
            }
            .onAppear {
                appDelegate.onOpenURL = { url in
                    print("Delegate open URL: \(url)")
                    openedURL = url
                    isReady = false
                    currentWindow?.orderOut(nil)
                }
            }
            .onReceive(NotificationCenter.default.publisher(for: .didDecodeImageSize)) { note in
                guard let size = note.userInfo?["size"] as? CGSize else { return }
                self.imageSize = size
                applyWindowSizeAndShow(size)
                isReady = true
            }
        }
    }

    private func applyWindowSizeAndShow(_ size: CGSize) {
        guard let window = currentWindow else { return }
        // Apply borderless styling
        window.titleVisibility = .visible
        //window.titlebarAppearsTransparent = true
        window.isMovableByWindowBackground = true
        //window.styleMask.remove(.titled)
        //window.styleMask.remove(.resizable)
        //window.toolbar = nil
        //window.isOpaque = false
        //window.backgroundColor = .clear

        // Clamp to screen
        let screenSize = NSScreen.main?.visibleFrame.size ?? size
        let clamped = CGSize(
            width: min(size.width, max(200, screenSize.width - 80)),
            height: min(size.height, max(200, screenSize.height - 80))
        )
        window.setContentSize(clamped)
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
}

// MARK: - Window accessor to resolve NSWindow
struct WindowAccessor: NSViewRepresentable {
    var onResolve: (NSWindow) -> Void
    func makeNSView(context: Context) -> NSView {
        let view = NSView()
        DispatchQueue.main.async {
            if let window = view.window { onResolve(window) }
        }
        return view
    }
    func updateNSView(_ nsView: NSView, context: Context) {}
}

// MARK: - Notification used to pass decoded size
extension Notification.Name {
    static let didDecodeImageSize = Notification.Name("PFR1.didDecodeImageSize")
}

