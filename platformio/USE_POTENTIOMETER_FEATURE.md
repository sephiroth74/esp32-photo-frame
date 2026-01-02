# USE_POTENTIOMETER Feature

## Overview

The `USE_POTENTIOMETER` feature allows you to choose between two modes for controlling the display refresh interval:

1. **Potentiometer Mode** (when `USE_POTENTIOMETER` is defined): Physical potentiometer hardware controls refresh interval
2. **Fixed Interval Mode** (when `USE_POTENTIOMETER` is not defined): Uses a fixed interval from `config.json`

## Configuration

### Board Configuration Header

In your board-specific header file (e.g., `include/config/pros3d_unexpectedmaker.h`):

```cpp
// Enable potentiometer mode
#define USE_POTENTIOMETER
#define POTENTIOMETER_PWR_PIN    7    // Power pin (or -1 for direct 3.3V)
#define POTENTIOMETER_INPUT_PIN  3    // Analog input pin
#define POTENTIOMETER_INPUT_MAX  4095 // 12-bit ADC range
```

Or leave it commented out for fixed interval mode:

```cpp
// Potentiometer disabled - using fixed interval from config.json
// #define USE_POTENTIOMETER
// #define POTENTIOMETER_PWR_PIN   7
// #define POTENTIOMETER_INPUT_PIN 3
// #define POTENTIOMETER_INPUT_MAX 4095
```

### JSON Configuration

In `config.json` (or `data/config.json`):

```json
{
  "board_config": {
    "refresh": {
      "min_seconds": 600,           // 10 minutes (potentiometer minimum)
      "max_seconds": 14400,         // 4 hours (potentiometer maximum)
      "step": 300,                  // 5 minutes (potentiometer step)
      "default": 1800,              // 30 minutes (used when USE_POTENTIOMETER not defined)
      "low_battery_multiplier": 3   // Multiplier applied during low battery
    }
  }
}
```

## Behavior

### With USE_POTENTIOMETER Defined

- Reads analog value from potentiometer pin
- Maps value to refresh interval between `min_seconds` and `max_seconds`
- Uses cubic exponential curve for fine control at lower values
- Rounds to nearest `step` increment
- Example mapping (with exponential curve):
  - 10% rotation → ~5 minutes
  - 25% rotation → ~9 minutes
  - 50% rotation → ~33 minutes
  - 75% rotation → ~106 minutes
  - 100% rotation → 240 minutes

### Without USE_POTENTIOMETER Defined

- Uses fixed interval from `config.board.refresh.default_seconds`
- Value must be between `min_seconds` and `max_seconds`
- Simpler configuration, no hardware required
- Still respects battery state multiplier

### Battery Behavior (Both Modes)

- **Critical Battery**: Always uses `REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY` (6 hours default)
- **Low Battery** (without potentiometer): Multiplies `default_seconds` by `low_battery_multiplier`
- **Low Battery** (with potentiometer): Uses critical battery interval

## Code Changes

### Modified Files

1. **config.h**
   - Added `USE_POTENTIOMETER` constant (checked with `#ifdef`)
   - Added `REFRESH_DEFAULT_INTERVAL_SECONDS` (30 minutes default)

2. **unified_config.h**
   - Added `default_seconds` field to `refresh_config` struct
   - Added validation: `default_seconds` must be within `[min_seconds, max_seconds]`

3. **unified_config.cpp**
   - Parses `"default"` field from JSON with fallback to `REFRESH_DEFAULT_INTERVAL_SECONDS`
   - Validates and clamps value to valid range

4. **board_util.h / board_util.cpp**
   - Modified `read_refresh_seconds()` signature to accept `const unified_config&`
   - Conditional implementation based on `USE_POTENTIOMETER`:
     - With: Reads potentiometer (original behavior)
     - Without: Uses `config.board.refresh.default_seconds`

5. **main.cpp**
   - Updated call to `read_refresh_seconds(systemConfig, battery_info.is_low())`

6. **display_debug.cpp**
   - Updated potentiometer test to load configuration
   - Shows default value when potentiometer not enabled
   - Updated call to pass configuration

7. **config.cpp**
   - Made potentiometer pin validation conditional on `USE_POTENTIOMETER`

## Migration Guide

### Existing Projects with Potentiometer

No changes needed! Simply ensure `USE_POTENTIOMETER` is defined in your board config.

### Existing Projects without Potentiometer

1. Leave `USE_POTENTIOMETER` undefined (or commented out)
2. Add `"default": 1800` to your `config.json` under `board_config.refresh`
3. The default interval will be used automatically

### New Projects

Choose your mode:

- **Hardware control**: Define `USE_POTENTIOMETER` and wire up a potentiometer
- **Fixed interval**: Leave undefined and set `"default"` in config.json

## Testing

Use the Display Debug menu (option 3) to test:

```
Display Debug Menu
==================
3 - Potentiometer Test
```

**With potentiometer enabled**: Shows real-time potentiometer readings and calculated intervals

**Without potentiometer**: Shows the configured default interval from config.json

## Example Output

### With Potentiometer
```
Potentiometer Test
==================
Reading potentiometer for 10 seconds...
Pin: GPIO3
ADC Range: 0-4095
Turn the potentiometer to see values change

Time | Refresh Interval
-----|------------------
   0 | 5 minutes
   2 | 15 minutes
   5 | 30 minutes
```

### Without Potentiometer
```
Potentiometer Test
==================
Potentiometer not enabled (USE_POTENTIOMETER not defined)
Using default refresh interval from config.json

Default refresh interval: 1800 seconds
                         30 minutes
```

## Validation

The system enforces:

- `default_seconds` must be >= `min_seconds`
- `default_seconds` must be <= `max_seconds`
- Configuration loading fails if these constraints are violated
- Fallback to `REFRESH_DEFAULT_INTERVAL_SECONDS` if JSON parsing fails
