//
//  ImportExtension.swift
//  PFR1MetadataImporter
//
//  Created by Alessandro Crugnola on 28.01.2026.
//

import CoreSpotlight
import UniformTypeIdentifiers
import OSLog

final class ImportExtension: CSImportExtension {
    
    private let logger = Logger(subsystem: "it.sephiroth.PFR1MetadataImporter", category: "ImportExtension")

    override func update(_ attributes: CSSearchableItemAttributeSet, forFileAt url: URL) throws {
        logger.info("update for file: \(url, privacy: .public)")
        
        // Leggi i dati del file
        let data = try Data(contentsOf: url)
        let header = try PFR1Decoder.decodeHeader(from: data)

        let version = header.version
        let headerLen = header.headerLen
        let width = header.width
        let height = header.height
        let rotation = header.rotation
        let colorMode = header.colorMode
        let payloadLen = header.payloadLen
        let headerCRC32 = header.headerCRC32


        // Popola attributi standard utili per Finder
        attributes.pixelWidth = NSNumber(value: Int(width))
        attributes.pixelHeight = NSNumber(value: Int(height))
        // Puoi aggiungere anche contentType se vuoi confermare
        if let type = try? url.resourceValues(forKeys: [.contentTypeKey]).contentType {
            logger.debug("type identifier: \(type.identifier)")
            attributes.identifier = type.identifier
        }
        

        // Attributi personalizzati (appariranno come metadati indicizzabili)
        // Nota: per una resa migliore nel pannello Info, valuta di dichiarare queste chiavi nel plist dell’estensione.
        let versionKey = CSCustomAttributeKey(keyName: "it.sephiroth.pfr1.version")!
        let rotationKey = CSCustomAttributeKey(keyName: "it.sephiroth.pfr1.rotation")!
        let colorModeKey = CSCustomAttributeKey(keyName: "it.sephiroth.pfr1.colorMode")!
        let payloadLenKey = CSCustomAttributeKey(keyName: "it.sephiroth.pfr1.payloadLength")!
        let headerLenKey = CSCustomAttributeKey(keyName: "it.sephiroth.pfr1.headerLength")!

        attributes.setValue(NSNumber(value: version), forCustomKey: versionKey)
        attributes.setValue(NSNumber(value: rotation), forCustomKey: rotationKey)
        attributes.setValue(NSNumber(value: colorMode), forCustomKey: colorModeKey)
        attributes.setValue(NSNumber(value: payloadLen), forCustomKey: payloadLenKey)
        attributes.setValue(NSNumber(value: headerLen), forCustomKey: headerLenKey)

        // Una descrizione riassuntiva che spesso Finder mostra
        attributes.contentDescription = "PFR1 v\(version) — \(width)x\(height) — rot: \(rotation) — mode: \(colorMode)"
    }
}

