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
    @State private var showInspector = false
    
    @State private var lastContentSize: CGSize? = nil
    @State private var showInfoPopover = false
    private let inspectorWidth: CGFloat = 300

    var body: some Scene {
        WindowGroup {
            HStack(spacing: 0) {
                Group {
                    if let url = openedURL {
                        // Instantiate the viewer immediately so it can decode and post size
                        PFR1ImageView(url: url, onImageSize: { _ in })
                            .id(url)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)

                if showInspector, let url = openedURL {
                    PFR1InspectorView(url: url)
                        .frame(width: inspectorWidth)
                        .transition(.move(edge: .trailing).combined(with: .opacity))
                }
            }
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button {
                        showInfoPopover.toggle()
                    } label: {
                        Image(systemName: "info.circle")
                    }
                    .help("Show file informations")
                    .disabled(openedURL == nil)
                    .popover(isPresented: $showInfoPopover, arrowEdge: .top) {
                        if let url = openedURL {
                            PFR1InfoPopoverView(url: url)
                                .frame(minWidth: 260, idealWidth: 320)
                                .padding()
                        } else {
                            Text("No opened file").padding()
                        }
                    }
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
        lastContentSize = clamped
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

struct PFR1InspectorView: View {
    let url: URL
    @State private var info: String = "Loading…"
    @State private var error: String?
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("File Info").font(.headline)
            if let error = error {
                Text(error).foregroundColor(.red)
            } else {
                Text(info).font(.system(.body, design: .monospaced))
            }
            Spacer()
        }
        .padding()
        .frame(minWidth: 260, maxWidth: 320, maxHeight: .infinity)
        .background(.ultraThickMaterial)
        .onAppear(perform: load)
        .onChange(of: url) { _ in load() }
    }
    private func load() {
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let data = try Data(contentsOf: url)
                let header = try PFR1Decoder.decodeHeader(from: data)
                let text = """
Name: \(url.lastPathComponent)
Size: \(data.count) bytes
Width: \(header.width)
Height: \(header.height)
Rotation: \(header.rotation)
Color Mode: \(header.colorMode)
Version: \(header.version)
Payload: \(header.payloadLen)
"""
                DispatchQueue.main.async { self.info = text; self.error = nil }
            } catch { DispatchQueue.main.async { self.error = error.localizedDescription } }
        }
    }
}
struct PFR1InfoPopoverView: View {
    let url: URL
    @State private var info: String = "Loading…"
    @State private var error: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("File informations").font(.headline)
            if let error = error {
                Text(error).foregroundColor(.red)
            } else {
                Text(info)
                    .font(.system(.body, design: .monospaced))
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
        .onAppear(perform: load)
        .onChange(of: url) { _ in load() }
    }

    private func load() {
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let data = try Data(contentsOf: url)
                let header = try PFR1Decoder.decodeHeader(from: data)
                let text = """
Name: \(url.lastPathComponent)
Size: \(data.count) bytes
Width: \(header.width)
Height: \(header.height)
Rotation: \(header.rotation)
Color: \(header.colorMode)
Version: \(header.version)
Payload: \(header.payloadLen)
"""
                DispatchQueue.main.async { self.info = text; self.error = nil }
            } catch { DispatchQueue.main.async { self.error = error.localizedDescription } }
        }
    }
}

