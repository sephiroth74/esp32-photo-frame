import SwiftUI
internal import AppKit

struct PFR1ImageView: View {
    let url: URL
    let onImageSize: ((CGSize) -> Void)?
    
    @State private var image: NSImage?
    @State private var error: String?
    
    var body: some View {
        Group {
            if let image = image {
                BorderlessNSImageView(image: image)
                    .frame(minWidth: 100, minHeight: 100)
            } else if let error = error {
                Text("Error: \(error)")
                    .foregroundColor(.red)
                    .multilineTextAlignment(.center)
                    .padding()
            } else {
                ProgressView()
                    .progressViewStyle(CircularProgressViewStyle())
                    .frame(minWidth: 100, minHeight: 100)
            }
        }
        .onAppear(perform: loadData)
        .onChange(of: url) { _ in
            image = nil
            error = nil
            loadData()
        }
    }
    
    private func loadData() {
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let data = try Data(contentsOf: url)
                let (decodedImage, _) = try PFR1Decoder.decode(from: data)
                DispatchQueue.main.async {
                    self.image = decodedImage
                    self.onImageSize?(decodedImage.size)
                    print("Posting didDecodeImageSize with: \(decodedImage.size)")
                    NotificationCenter.default.post(name: .didDecodeImageSize, object: nil, userInfo: ["size": decodedImage.size])
                }
            } catch {
                DispatchQueue.main.async {
                    self.error = error.localizedDescription
                }
            }
        }
    }
}

struct BorderlessNSImageView: NSViewRepresentable {
    let image: NSImage
    
    func makeNSView(context: Context) -> NSImageView {
        let imageView = NSImageView()
        imageView.image = image
        imageView.imageScaling = .scaleProportionallyUpOrDown
        imageView.cell?.isBordered = false
        imageView.wantsLayer = true
        imageView.layer?.borderWidth = 0
        imageView.layer?.borderColor = nil
        return imageView
    }
    
    func updateNSView(_ nsView: NSImageView, context: Context) {
        nsView.image = image
    }
}

