//
//  ThumbnailProvider.swift
//  CustomQLThumbnailProvider
//
//  Created by Itsuki on 2025/09/29.
//

import QuickLookThumbnailing
import OSLog
internal import AppKit

private let logger = Logger(subsystem: "it.sephiroth.CustomQLThumbnailProvider", category: "Thumbnail")

final class ThumbnailProvider: QLThumbnailProvider {

    override func provideThumbnail(for request: QLFileThumbnailRequest,
                                   _ handler: @escaping (QLThumbnailReply?, Error?) -> Void) {
        logger.error("Thumbnail provider invoked for: \(request.fileURL.lastPathComponent, privacy: .public)")
        logger.debug("provideThumbnail called for: \(request.fileURL.lastPathComponent, privacy: .public)")
        if let values = try? request.fileURL.resourceValues(forKeys: [.contentTypeKey]),
           let type = values.contentType {
            logger.debug("Requested UTType: \(type.identifier, privacy: .public)")
        } else {
            logger.debug("Requested UTType: <unknown>")
        }
        logger.debug("Maximum size requested: \(String(describing: request.maximumSize), privacy: .public)")

        do {
            let data = try Data(contentsOf: request.fileURL)
            logger.debug("Loaded data: \(data.count) bytes")
            let (image, _) = try PFR1Decoder.decode(from: data)
            logger.debug("Decoded image size: \(String(describing: image.size), privacy: .public)")

            let imageSize = image.size
            let maxSize = request.maximumSize
            let scale = min(maxSize.width / imageSize.width, maxSize.height / imageSize.height)
            let targetSize = CGSize(width: max(1, imageSize.width * scale),
                                    height: max(1, imageSize.height * scale))
            logger.debug("Target thumbnail size: \(String(describing: targetSize), privacy: .public)")

            let reply = QLThumbnailReply(contextSize: targetSize, currentContextDrawing: {
                image.draw(in: NSRect(origin: .zero, size: targetSize))
                return true
            })

            handler(reply, nil)
        } catch {
            logger.error("Thumbnail error: \(String(describing: error), privacy: .public)")
            handler(nil, error)
        }
    }
}

