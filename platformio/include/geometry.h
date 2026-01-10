#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include <Arduino.h>

/**
 * @brief Represents a rectangular region with position and dimensions.
 *
 * This structure defines a rectangle with x,y coordinates for position
 * and width,height for dimensions. It provides utility methods for
 * geometric operations like intersection testing and containment checks.
 */
typedef struct rect {
    int16_t x;       ///< X-coordinate of the rectangle's top-left corner
    int16_t y;       ///< Y-coordinate of the rectangle's top-left corner
    uint16_t width;  ///< Width of the rectangle in pixels
    uint16_t height; ///< Height of the rectangle in pixels

  public:
    /**
     * @brief Checks if the rectangle has positive dimensions.
     * @return True if both width and height are greater than 0
     */
    operator bool() const { return (width > 0 && height > 0); }

    /**
     * @brief Checks if the rectangle is empty (zero width or height).
     * @return True if width or height is zero
     */
    inline constexpr bool is_empty() const { return (width == 0 || height == 0); }

    /**
     * @brief Checks if the rectangle is valid (positive position and dimensions).
     * @return True if x >= 0, y >= 0, width > 0, and height > 0
     */
    inline constexpr bool is_valid() const { return (x >= 0 && y >= 0 && width > 0 && height > 0); }

    /**
     * @brief Checks if a point is contained within the rectangle.
     * @param px X-coordinate of the point to test
     * @param py Y-coordinate of the point to test
     * @return True if the point is inside the rectangle
     */
    inline constexpr bool contains(int16_t px, int16_t py) const {
        return (px >= x && px < x + width && py >= y && py < y + height);
    }

    /**
     * @brief Checks if this rectangle intersects with another rectangle.
     * @param other The other rectangle to test intersection with
     * @return True if the rectangles overlap
     */
    inline constexpr bool intersects(const rect& other) const {
        return (x < other.x + other.width && x + width > other.x && y < other.y + other.height &&
                y + height > other.y);
    }

    /**
     * @brief Constructor for rectangle with optional parameters.
     * @param x X-coordinate of top-left corner (default: 0)
     * @param y Y-coordinate of top-left corner (default: 0)
     * @param width Width in pixels (default: 0)
     * @param height Height in pixels (default: 0)
     */
    constexpr rect(int16_t x = 0, int16_t y = 0, uint16_t width = 0, uint16_t height = 0) :
        x(x),
        y(y),
        width(width),
        height(height) {}

    /**
     * @brief Static factory method to create a rectangle from coordinates and dimensions.
     * @param x X-coordinate of top-left corner
     * @param y Y-coordinate of top-left corner
     * @param width Width in pixels
     * @param height Height in pixels
     * @return A new rect instance
     */
    static inline constexpr rect from_xywh(int16_t x, int16_t y, uint16_t width, uint16_t height) {
        return rect(x, y, width, height);
    }

    /**
     * @brief Static factory method to create an empty rectangle.
     * @return A rect with all dimensions set to 0
     */
    static inline constexpr rect empty() { return rect(0, 0, 0, 0); }

} rect_t;

/**
 * @brief Text alignment options for rendering.
 *
 * Defines how text should be aligned relative to its position coordinates.
 */
typedef enum alignment {
    LEFT,  ///< Align text to the left of the position
    RIGHT, ///< Align text to the right of the position
    CENTER ///< Center text at the position
} alignment_t;

/**
 * @brief Gravity options for positioning elements on the display.
 *
 * Defines anchor points for positioning UI elements relative to
 * the display boundaries.
 */
typedef enum gravity {
    TOP_LEFT,     ///< Position at top-left corner
    TOP_RIGHT,    ///< Position at top-right corner
    BOTTOM_LEFT,  ///< Position at bottom-left corner
    BOTTOM_RIGHT, ///< Position at bottom-right corner
    TOP_CENTER,   ///< Position at top-center
} gravity_t;

#endif // __GEOMETRY_H__