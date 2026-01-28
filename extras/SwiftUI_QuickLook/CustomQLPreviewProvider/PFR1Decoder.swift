internal import AppKit
import Foundation

// Simple PFR1 decoder for Quick Look usage.
// Assumes payload is 1 byte per pixel (0x00 black, 0xFF white).
struct PFR1Header {
    let magic: UInt32
    let version: UInt8
    let headerLen: UInt16
    let width: UInt16
    let height: UInt16
    let rotation: UInt8
    let colorMode: UInt8
    let payloadLen: UInt32
    let headerCRC32: UInt32
}

enum PFR1DecoderError: Error {
    case tooSmall
    case badMagic
    case badHeaderCRC
    case payloadTooShort
    case unsupportedColorMode(UInt8)
}

final class PFR1Decoder {
    private static let headerSize = 21
    private static let magic: UInt32 = 0x5046_5231  // 'PFR1' LE

    // Map byte values to RGB colors for 6-color mode
    // Based on rgb_to_demo_bitmap_mode1 in photoframe-lib/src/core.rs
    private static func colorFrom6CValue(_ value: UInt8) -> (r: UInt8, g: UInt8, b: UInt8) {
        switch value {
        case 0x00:  // Black
            return (0x00, 0x00, 0x00)
        case 0x03:  // Blue
            return (0x00, 0x00, 0xFF)
        case 0x1C:  // Green
            return (0x00, 0xFF, 0x00)
        case 0xE0:  // Red
            return (0xFF, 0x00, 0x00)
        case 0xFC:  // Yellow
            return (0xFF, 0xFF, 0x00)
        case 0xFF:  // White
            return (0xFF, 0xFF, 0xFF)
        default:  // Unknown value, default to black
            return (0x00, 0x00, 0x00)
        }
    }

    static func decode(from data: Data) throws -> (NSImage, PFR1Header) {
        guard data.count >= headerSize else { throw PFR1DecoderError.tooSmall }

        let magic = data.readUInt32LE(at: 0)
        guard magic == self.magic else { throw PFR1DecoderError.badMagic }

        let version = data[4]
        let headerLen = data.readUInt16LE(at: 5)
        let width = data.readUInt16LE(at: 7)
        let height = data.readUInt16LE(at: 9)
        let rotation = data[11]
        let colorMode = data[12]
        let payloadLen = data.readUInt32LE(at: 13)
        let headerCRC32 = data.readUInt32LE(at: 17)

        // Validate header CRC32 (over first headerLen-4 bytes)
        let headerSlice = data.prefix(Int(headerLen - 4))
        let crc = headerSlice.crc32()
        guard crc == headerCRC32 else { throw PFR1DecoderError.badHeaderCRC }

        let payloadStart = Int(headerLen)
        guard data.count >= payloadStart + Int(payloadLen) else {
            throw PFR1DecoderError.payloadTooShort
        }

        let payload = data.subdata(in: payloadStart..<payloadStart + Int(payloadLen))

        // Payload must have exactly width * height bytes (1 byte per pixel)
        let pixelCount = Int(width) * Int(height)
        guard payload.count == pixelCount else {
            throw PFR1DecoderError.payloadTooShort
        }

        // For 6C mode, we need to convert color codes to RGB
        // For BW mode, we can use the bytes directly as grayscale
        let image: NSImage

        if colorMode == 1 {
            // 6C mode: convert color codes to RGB (need 3 bytes per pixel)
            let bytesPerPixel = 3
            var rgbData = Data(count: pixelCount * bytesPerPixel)

            rgbData.withUnsafeMutableBytes { outPtr in
                guard let dst = outPtr.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                    return
                }
                payload.withUnsafeBytes { srcPtr in
                    guard let src = srcPtr.baseAddress?.assumingMemoryBound(to: UInt8.self) else {
                        return
                    }

                    for i in 0..<pixelCount {
                        let colorValue = src[i]
                        let color = Self.colorFrom6CValue(colorValue)
                        let base = i * bytesPerPixel
                        dst[base + 0] = color.r
                        dst[base + 1] = color.g
                        dst[base + 2] = color.b
                    }
                }
            }

            let bitmap = NSBitmapImageRep(
                bitmapDataPlanes: nil,
                pixelsWide: Int(width),
                pixelsHigh: Int(height),
                bitsPerSample: 8,
                samplesPerPixel: 3,
                hasAlpha: false,
                isPlanar: false,
                colorSpaceName: .deviceRGB,
                bytesPerRow: Int(width) * bytesPerPixel,
                bitsPerPixel: 24
            )

            rgbData.withUnsafeBytes { srcPtr in
                if let dst = bitmap?.bitmapData {
                    dst.assign(
                        from: srcPtr.bindMemory(to: UInt8.self).baseAddress!, count: rgbData.count)
                }
            }

            image = NSImage(size: NSSize(width: Int(width), height: Int(height)))
            image.addRepresentation(bitmap!)

        } else {
            // BW mode: use payload directly as grayscale (1 byte per pixel)
            let bitmap = NSBitmapImageRep(
                bitmapDataPlanes: nil,
                pixelsWide: Int(width),
                pixelsHigh: Int(height),
                bitsPerSample: 8,
                samplesPerPixel: 1,
                hasAlpha: false,
                isPlanar: false,
                colorSpaceName: .deviceWhite,
                bytesPerRow: Int(width),
                bitsPerPixel: 8
            )

            payload.withUnsafeBytes { srcPtr in
                if let dst = bitmap?.bitmapData {
                    dst.assign(
                        from: srcPtr.bindMemory(to: UInt8.self).baseAddress!, count: payload.count)
                }
            }

            image = NSImage(size: NSSize(width: Int(width), height: Int(height)))
            image.addRepresentation(bitmap!)
        }

        // Apply rotation if needed
        if rotation != 0 {
            if let rotated = image.rotated(byDegrees: Double(rotation) * 90.0) {
                return (
                    rotated,
                    PFR1Header(
                        magic: magic, version: version, headerLen: headerLen, width: width,
                        height: height, rotation: rotation, colorMode: colorMode,
                        payloadLen: payloadLen, headerCRC32: headerCRC32)
                )
            }
        }

        return (
            image,
            PFR1Header(
                magic: magic, version: version, headerLen: headerLen, width: width, height: height,
                rotation: rotation, colorMode: colorMode, payloadLen: payloadLen,
                headerCRC32: headerCRC32)
        )
    }
    
    static func decodeHeader(from data: Data) throws -> PFR1Header {
        guard data.count >= headerSize else { throw PFR1DecoderError.tooSmall }

        let magic = data.readUInt32LE(at: 0)
        guard magic == self.magic else { throw PFR1DecoderError.badMagic }

        let version = data[4]
        let headerLen = data.readUInt16LE(at: 5)
        let width = data.readUInt16LE(at: 7)
        let height = data.readUInt16LE(at: 9)
        let rotation = data[11]
        let colorMode = data[12]
        let payloadLen = data.readUInt32LE(at: 13)
        let headerCRC32 = data.readUInt32LE(at: 17)

        // Validate header CRC32 (over first headerLen-4 bytes)
        let headerSlice = data.prefix(Int(headerLen - 4))
        let crc = headerSlice.crc32()
        guard crc == headerCRC32 else { throw PFR1DecoderError.badHeaderCRC }

        let payloadStart = Int(headerLen)
        guard data.count >= payloadStart + Int(payloadLen) else {
            throw PFR1DecoderError.payloadTooShort
        }

        let payload = data.subdata(in: payloadStart..<payloadStart + Int(payloadLen))

        // Payload must have exactly width * height bytes (1 byte per pixel)
        let pixelCount = Int(width) * Int(height)
        guard payload.count == pixelCount else {
            throw PFR1DecoderError.payloadTooShort
        }


        return
            PFR1Header(
                magic: magic, version: version, headerLen: headerLen, width: width, height: height,
                rotation: rotation, colorMode: colorMode, payloadLen: payloadLen,
                headerCRC32: headerCRC32)
        
    }
}

extension Data {
    fileprivate func readUInt16LE(at offset: Int) -> UInt16 {
        let lo = UInt16(self[offset])
        let hi = UInt16(self[offset + 1]) << 8
        return hi | lo
    }

    fileprivate func readUInt32LE(at offset: Int) -> UInt32 {
        let b0 = UInt32(self[offset])
        let b1 = UInt32(self[offset + 1]) << 8
        let b2 = UInt32(self[offset + 2]) << 16
        let b3 = UInt32(self[offset + 3]) << 24
        return b3 | b2 | b1 | b0
    }

    // Simple CRC32 (polynomial 0xEDB88320)
    fileprivate func crc32() -> UInt32 {
        var crc: UInt32 = 0xFFFF_FFFF
        for byte in self {
            crc ^= UInt32(byte)
            for _ in 0..<8 {
                let mask = -(Int(crc) & 1)
                crc = (crc >> 1) ^ UInt32((0xEDB8_8320 & mask))
            }
        }
        return ~crc
    }
}

//extension NSImage {
//    fileprivate func rotated(byDegrees degrees: Double) -> NSImage? {
//        guard degrees.truncatingRemainder(dividingBy: 360) != 0 else { return self }
//        let imageRotated = NSImage(size: size)
//        imageRotated.lockFocus()
//        let transform = NSAffineTransform()
//        transform.translateX(by: size.width / 2, yBy: size.height / 2)
//        transform.rotate(byDegrees: CGFloat(degrees))
//        transform.translateX(by: -size.width / 2, yBy: -size.height / 2)
//        transform.concat()
//        draw(
//            at: NSZeroPoint, from: NSRect(origin: .zero, size: size), operation: .copy,
//            fraction: 1.0)
//        imageRotated.unlockFocus()
//        return imageRotated
//    }
//}

extension NSImage {
    fileprivate func rotated(byDegrees degrees: Double) -> NSImage? {
        let angle = (Int(degrees) % 360 + 360) % 360
        guard angle != 0 else { return self }

        let canvasSize: CGSize
        switch angle {
        case 90, 270:
            canvasSize = CGSize(width: self.size.height, height: self.size.width)
        case 180:
            canvasSize = self.size
        default:
            // fallback per angoli non multipli di 90°
            let radians = CGFloat(degrees * .pi / 180)
            let originalRect = CGRect(origin: .zero, size: self.size)
            let rotatedBounds = originalRect.applying(CGAffineTransform(rotationAngle: radians))
            canvasSize = CGSize(width: abs(rotatedBounds.width), height: abs(rotatedBounds.height))
        }

        let rotatedImage = NSImage(size: canvasSize)
        rotatedImage.lockFocus()
        let ctx = NSGraphicsContext.current?.cgContext

        ctx?.translateBy(x: canvasSize.width / 2, y: canvasSize.height / 2)
        ctx?.rotate(by: CGFloat(degrees * .pi / 180))

        let drawRect = CGRect(x: -self.size.width / 2,
                              y: -self.size.height / 2,
                              width: self.size.width,
                              height: self.size.height)
        self.draw(in: drawRect)

        rotatedImage.unlockFocus()
        return rotatedImage
    }
}
