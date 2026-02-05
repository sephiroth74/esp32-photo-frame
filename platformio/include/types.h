#ifndef PHOTO_FRAME_TYPES_H
#define PHOTO_FRAME_TYPES_H

namespace photo_frame {
/**
 * @brief Image source enumeration for tracking where an image was loaded from.
 */
enum ImageSource {
    IMAGE_SOURCE_CLOUD,       ///< Image was downloaded from Google Drive
    IMAGE_SOURCE_LOCAL_CACHE, ///< Image was loaded from local SD card cache
    IMAGE_SOURCE_BLUETOOTH,   ///< Image was received via Bluetooth
    IMAGE_SOURCE_WEBSOCKET,   ///< Image was received via WebSocket
    IMAGE_SOURCE_NONE,
};

} // namespace photo_frame

#endif // PHOTO_FRAME_TYPES_H