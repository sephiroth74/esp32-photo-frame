//
//  PreviewProvider.swift
//  PFR1QuickLookExtension
//
//  Created by Alessandro Crugnola on 27.01.2026.
//

import Cocoa
import Quartz
import QuickLookThumbnailing

class PreviewProvider: QLPreviewProvider, QLPreviewingController {

    func providePreview(
        for request: QLFilePreviewRequest,
        completionHandler: @escaping (QLPreviewReply?, Error?) -> Void
    ) {
        do {
            let data = try Data(contentsOf: request.fileURL)
            let (image, _) = try PFR1Decoder.decode(from: data)

            guard let tiff = image.tiffRepresentation,
                let rep = NSBitmapImageRep(data: tiff),
                let png = rep.representation(using: .png, properties: [:])
            else {
                throw NSError(
                    domain: "PFR1QL", code: -1,
                    userInfo: [NSLocalizedDescriptionKey: "Failed to build PNG"])
            }

            // Use the actual image size to preserve aspect ratio
            let reply = QLPreviewReply(dataOfContentType: .png, contentSize: image.size) { _ in
                return png
            }
            completionHandler(reply, nil)
        } catch {
            completionHandler(nil, error)
        }
    }

    func provideThumbnail(
        for request: QLFileThumbnailRequest,
        _ handler: @escaping (QLThumbnailReply?, Error?) -> Void
    ) {
        do {
            let data = try Data(contentsOf: request.fileURL)
            let (image, _) = try PFR1Decoder.decode(from: data)

            // Calculate size that fits in maximumSize while preserving aspect ratio
            let imageSize = image.size
            let maxSize = request.maximumSize
            let scale = min(maxSize.width / imageSize.width, maxSize.height / imageSize.height)
            let targetSize = CGSize(
                width: imageSize.width * scale, height: imageSize.height * scale)

            let reply = QLThumbnailReply(
                contextSize: targetSize,
                currentContextDrawing: { () -> Bool in
                    image.draw(in: NSRect(origin: .zero, size: targetSize))
                    return true
                })
            handler(reply, nil)
        } catch {
            handler(nil, error)
        }
    }
}
