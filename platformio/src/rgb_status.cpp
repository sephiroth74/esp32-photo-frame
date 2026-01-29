// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rgb_status.h"
#include "config.h"

#ifdef RGB_STATUS_ENABLED
// Global RGB status instance
RGBStatus rgbStatus;

// Predefined status configurations - optimized for power efficiency (max brightness 64)
const StatusConfig RGBStatus::STATUS_CONFIGS[] = {
    {SystemState::IDLE,            RGBColors::DARK_BLUE, RGBEffect::SOLID,      0,    12},
    {SystemState::STARTING,        RGBColors::WHITE,     RGBEffect::PULSE,      3000, 12},
    {SystemState::WIFI_CONNECTING, RGBColors::BLUE,      RGBEffect::PULSE,      0,    12},
    {SystemState::WIFI_FAILED,     RGBColors::RED,       RGBEffect::BLINK_SLOW, 0,    12},
    {SystemState::SD_READING,      RGBColors::ORANGE,    RGBEffect::PULSE,      0,    12},
    {SystemState::SD_WRITING,      RGBColors::YELLOW,    RGBEffect::PULSE,      0,    12},
    {SystemState::GOOGLE_DRIVE,    RGBColors::CYAN,      RGBEffect::PULSE,      0,    12},
    {SystemState::DOWNLOADING,     RGBColors::PURPLE,    RGBEffect::PULSE,      0,    12},
    {SystemState::RENDERING,       RGBColors::PINK,      RGBEffect::PULSE,      0,    12},
    {SystemState::BATTERY_LOW,     RGBColors::RED,       RGBEffect::BLINK_SLOW, 0,    12},
    {SystemState::ERROR,
     RGBColors::RED,
     RGBEffect::BLINK_FAST,
     0,                                                                               48}, // Keep error reasonably bright for visibility
    {SystemState::SLEEP_PREP,      RGBColors::DIM_WHITE, RGBEffect::FADE_OUT,   2000, 12},
#ifdef ENABLE_BT_IMAGE
    {SystemState::BT_WAITING,      RGBColors::CYAN,      RGBEffect::PULSE,      0,    12},
    {SystemState::BT_CONNECTED,    RGBColors::BLUE,      RGBEffect::SOLID,      0,    12},
    {SystemState::BT_RECEIVING,    RGBColors::GREEN,     RGBEffect::PULSE,      0,    12},
#endif  // ENABLE_BT_IMAGE
    {SystemState::CUSTOM,          RGBColors::WHITE,     RGBEffect::SOLID,      0,    12}
};

const size_t RGBStatus::NUM_STATUS_CONFIGS = sizeof(STATUS_CONFIGS) / sizeof(StatusConfig);

RGBStatus::RGBStatus() :
    pixels(nullptr),
    rgbTaskHandle(nullptr),
    currentState(SystemState::IDLE),
    currentConfig(SystemState::IDLE, RGBColors::OFF),
    enabled(true),
    taskRunning(false),
    lastUpdate(0),
    effectStep(0),
    currentBrightness(0) {}

RGBStatus::~RGBStatus() { end(); }

bool RGBStatus::begin() {
#ifdef LED_PWR_PIN
    // Enable power to RGB LED first
    log_i("Enabling LED power on GPIO%d", LED_PWR_PIN);
    pinMode(LED_PWR_PIN, OUTPUT);
    digitalWrite(LED_PWR_PIN, HIGH); // Power on
    delay(50);                       // Wait for power stabilization
#endif

    // Initialize NeoPixel
    pixels = new Adafruit_NeoPixel(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
    if (!pixels) {
        log_e("Failed to create NeoPixel object");
#ifdef LED_PWR_PIN
        digitalWrite(LED_PWR_PIN, LOW); // Power off on failure
#endif
        return false;
    }

    delay(10); // Allow additional stabilization

    pixels->begin();
    pixels->clear();
    pixels->show();

    log_i("NeoPixel initialized");

    // Create FreeRTOS task for RGB control
    BaseType_t result = xTaskCreatePinnedToCore(rgbTask,         // Task function
                                                "RGBStatusTask", // Task name
                                                2048,            // Stack size (bytes)
                                                this,            // Task parameter (this instance)
                                                1,               // Priority (1 = low priority)
                                                &rgbTaskHandle,  // Task handle
                                                0                // Core ID (0 = any core)
    );

    if (result != pdPASS) {
        log_e("Failed to create RGB task");
        delete pixels;
        pixels = nullptr;
        return false;
    }

    taskRunning = true;
    log_i("RGB status task started");

    // Set initial state
    setState(SystemState::STARTING, 1000);

    return true;
}

void RGBStatus::end() {
    if (rgbTaskHandle) {
        taskRunning = false;
        vTaskDelete(rgbTaskHandle);
        rgbTaskHandle = nullptr;
    }

    if (pixels) {
        pixels->clear();
        pixels->show();
        delete pixels;
        pixels = nullptr;
    }

#ifdef LED_PWR_PIN
    // Power off LED
    digitalWrite(LED_PWR_PIN, LOW);
    log_i("LED power disabled");
#endif

    log_i("RGB status system stopped");
}

void RGBStatus::setState(SystemState state, uint16_t duration_ms) {
    if (!enabled || !pixels)
        return;

    log_i("Setting state to %d", (int)state);

    // Find configuration for this state
    const StatusConfig* config = nullptr;
    for (size_t i = 0; i < NUM_STATUS_CONFIGS; i++) {
        if (STATUS_CONFIGS[i].state == state) {
            config = &STATUS_CONFIGS[i];
            break;
        }
    }

    if (!config) {
        log_w("Warning: No configuration found for state %d", (int)state);
        return;
    }

    currentState  = state;
    currentConfig = *config;
    if (duration_ms > 0) {
        currentConfig.duration_ms = duration_ms;
    }

    // Reset effect timing
    lastUpdate = millis();
    effectStep = 0;

    log_i("State changed to: %d", (int)state);
}

void RGBStatus::setCustomColor(const RGBColor& color,
                               RGBEffect effect,
                               uint16_t duration_ms,
                               uint8_t brightness) {
    if (!enabled || !pixels)
        return;

    currentState  = SystemState::CUSTOM;
    currentConfig = StatusConfig(SystemState::CUSTOM, color, effect, duration_ms, brightness);

    // Reset effect timing
    lastUpdate = millis();
    effectStep = 0;

    log_i("Custom color set: (%d,%d,%d) Effect: %d", color.r, color.g, color.b, (int)effect);
}

void RGBStatus::setBrightness(uint8_t brightness) { currentConfig.brightness = brightness; }

void RGBStatus::enable(bool en) {
    enabled = en;
    if (!enabled && pixels) {
        pixels->clear();
        pixels->show();
    }
}

void RGBStatus::turnOff() {
    if (pixels) {
        pixels->clear();
        pixels->show();
    }

#ifdef LED_PWR_PIN
    // Power off LED
    digitalWrite(LED_PWR_PIN, LOW);
    log_i("LED power disabled");
#endif

    currentState  = SystemState::IDLE;
    currentConfig = StatusConfig(SystemState::IDLE, RGBColors::OFF, RGBEffect::OFF);
}

// Static FreeRTOS task function
void RGBStatus::rgbTask(void* parameter) {
    RGBStatus* rgb = static_cast<RGBStatus*>(parameter);

    log_i("RGB task started");

    while (rgb->taskRunning) {
        if (rgb->enabled && rgb->pixels) {
            rgb->updateEffect();
        }

        // Task runs at ~50Hz for smooth effects
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    log_i("RGB task ended");
    vTaskDelete(NULL);
}

void RGBStatus::updateEffect() {
    unsigned long now = millis();

    // Check if effect duration has expired
    if (currentConfig.duration_ms > 0 && (now - lastUpdate) >= currentConfig.duration_ms) {
        setState(SystemState::IDLE);
        return;
    }

    uint8_t finalBrightness = currentConfig.brightness;
    RGBColor displayColor   = currentConfig.color;

    // Apply effect based on configured effect type
    switch (currentConfig.effect) {
    case RGBEffect::OFF:
        // Turn off completely
        setPixelColor(RGBColors::OFF, 0);
        return;

    case RGBEffect::SOLID:
        // Just display the color at full configured brightness
        setPixelColor(displayColor, finalBrightness);
        break;

    case RGBEffect::PULSE: {
        // Breathing effect - brightness varies smoothly
        uint8_t pulseBrightness = calculatePulse(effectStep, 120);
        uint8_t minBrightness   = currentConfig.brightness / 3; // Pulse to 1/3 brightness
        uint8_t maxBrightness   = currentConfig.brightness;

        // Map pulse (0-255) to brightness range
        finalBrightness = minBrightness + ((maxBrightness - minBrightness) * pulseBrightness) / 255;
        setPixelColor(displayColor, finalBrightness);
        break;
    }

    case RGBEffect::BLINK_SLOW: {
        // Slow blink: on for 500ms, off for 500ms
        uint32_t elapsed = now - lastUpdate;
        if ((elapsed / 500) % 2 == 0) {
            setPixelColor(displayColor, finalBrightness);
        } else {
            setPixelColor(RGBColors::OFF, 0);
        }
        break;
    }

    case RGBEffect::BLINK_FAST: {
        // Fast blink: on for 250ms, off for 250ms
        uint32_t elapsed = now - lastUpdate;
        if ((elapsed / 250) % 2 == 0) {
            setPixelColor(displayColor, finalBrightness);
        } else {
            setPixelColor(RGBColors::OFF, 0);
        }
        break;
    }

    case RGBEffect::FADE_IN: {
        // Fade in over duration time
        if (currentConfig.duration_ms > 0) {
            uint32_t elapsed = now - lastUpdate;
            finalBrightness  = (currentConfig.brightness * elapsed) / currentConfig.duration_ms;
            finalBrightness  = min(finalBrightness, currentConfig.brightness);
        }
        setPixelColor(displayColor, finalBrightness);
        break;
    }

    case RGBEffect::FADE_OUT: {
        // Fade out over duration time
        if (currentConfig.duration_ms > 0) {
            uint32_t elapsed = now - lastUpdate;
            finalBrightness  = currentConfig.brightness -
                              ((currentConfig.brightness * elapsed) / currentConfig.duration_ms);
            finalBrightness = max((uint8_t)0, finalBrightness);
        }
        setPixelColor(displayColor, finalBrightness);
        break;
    }

    case RGBEffect::RAINBOW: {
        // Rainbow color cycling with pulse effect
        RGBColor rainbowColor   = rainbow(effectStep / 4);
        uint8_t pulseBrightness = calculatePulse(effectStep, 120);
        uint8_t minBrightness   = currentConfig.brightness / 3;
        uint8_t maxBrightness   = currentConfig.brightness;

        finalBrightness = minBrightness + ((maxBrightness - minBrightness) * pulseBrightness) / 255;
        setPixelColor(rainbowColor, finalBrightness);
        break;
    }

    default:
        // Unknown effect, just turn off
        setPixelColor(RGBColors::OFF, 0);
        break;
    }

    // Update animation step
    effectStep = (effectStep + 1) % 1024; // Large enough for all effects
}

void RGBStatus::setPixelColor(const RGBColor& color, uint8_t brightness) {
    if (!pixels)
        return;

    // Apply brightness scaling
    uint8_t r = (color.r * brightness) / 255;
    uint8_t g = (color.g * brightness) / 255;
    uint8_t b = (color.b * brightness) / 255;

    pixels->setPixelColor(0, pixels->Color(r, g, b));
    pixels->show();
}

uint8_t RGBStatus::calculatePulse(uint16_t step, uint16_t period) {
    // Generate sine wave pulse effect
    float angle = (2.0 * PI * step) / period;
    float sine  = sin(angle);
    return (uint8_t)((sine + 1.0) * 127.5); // Convert -1,1 to 0,255
}

RGBColor RGBStatus::rainbow(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return RGBColor(255 - pos * 3, 0, pos * 3);
    } else if (pos < 170) {
        pos -= 85;
        return RGBColor(0, pos * 3, 255 - pos * 3);
    } else {
        pos -= 170;
        return RGBColor(pos * 3, 255 - pos * 3, 0);
    }
}

#endif // RGB_STATUS_ENABLED