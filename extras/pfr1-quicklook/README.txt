PFR1 Quick Look Extension (macOS)
=================================

What this is
------------
A Quick Look extension that renders .pfr1 files (PhotoFrame format) with thumbnails and previews in Finder.

Files
-----
- Info.plist               : Extension metadata
- PreviewProvider.swift    : QLPreviewProvider + QLThumbnailProvider entry point
- PFR1Decoder.swift        : Minimal decoder to turn PFR1 payload into an NSImage

How to integrate
----------------
1) Create an Xcode project (macOS App + Quick Look extension target) or a standalone Quick Look extension target.
2) Add these three files to the extension target.
3) In the extension target Info.plist, ensure:
   - NSExtensionPointIdentifier = com.apple.quicklook.preview
   - QLSupportsThumbnailGeneration = YES
   - QLSupportsPreview = YES
   - QLPreviewContentTypes includes it.sephiroth.photoframe.pfr1
4) In the host app (or a small helper app), register the UTI in Info.plist:
   <key>UTExportedTypeDeclarations</key>
   <array>
     <dict>
       <key>UTTypeIdentifier</key><string>it.sephiroth.photoframe.pfr1</string>
       <key>UTTypeDescription</key><string>PhotoFrame Image</string>
       <key>UTTypeConformsTo</key><array><string>public.image</string><string>public.data</string></array>
       <key>UTTypeTagSpecification</key>
         <dict><key>public.filename-extension</key><array><string>pfr1</string></array></dict>
     </dict>
   </array>
5) Build & archive the extension, then install the .appex in ~/Library/QuickLook/ or via the host app bundle.

Notes
-----
- Decoder assumes 1 byte per pixel payload; it maps to grayscale RGBA. If you add 6-color palettes later, extend PFR1Decoder.
- Rotation field is applied in 90° steps.
- For best results, keep payload_len <= actual file size; CRC32 is validated on header only.
