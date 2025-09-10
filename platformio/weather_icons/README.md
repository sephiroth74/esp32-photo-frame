# Weather Icons - Extracted

This directory contains individual weather icons automatically extracted from the combined SVG.

## Available Icons

- `sun.svg` - 6 paths (complex)
- `partly_cloudy.svg` - 13 paths (sun)
- `cloudy.svg` - 13 paths (sun)
- `overcast.svg` - 10 paths (sun)
- `light_rain.svg` - 10 paths (sun)
- `heavy_rain.svg` - 12 paths (sun)
- `drizzle.svg` - 13 paths (sun)
- `showers.svg` - 6 paths (complex)
- `thunderstorm.svg` - 8 paths (sun)
- `lightning.svg` - 5 paths (cloud_or_rain)

## Preview

Open `preview.html` in a web browser to see all icons.

## Usage in ESP32 Photo Frame

1. **Select the best icons** for your weather conditions
2. **Convert to PNG** at 16x16 pixels (to match status bar size)
3. **Convert to bitmap headers** using the existing icon pipeline
4. **Update weather mapping** in `weather.cpp`

### Conversion Commands

```bash
# Convert selected icons to 16x16 PNG
convert sun.svg -resize 16x16 -background white -flatten sun_16x16.png
convert cloudy.svg -resize 16x16 -background white -flatten cloudy_16x16.png
# ... repeat for other icons

# Use existing icon generation pipeline
python3 icons/final_generate_icons_h.py
```

### Integration

Update the `weather_icon_to_system_icon()` function in `src/weather.cpp` to map weather conditions to your new icons instead of the placeholder system icons.
