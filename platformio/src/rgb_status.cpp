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
    {SystemState::IDLE,             RGBColors::DARK_BLUE,  RGBEffect::SOLID,      0,    24},
    {SystemState::STARTING,         RGBColors::WHITE,      RGBEffect::PULSE,      3000, 64},
    {SystemState::WIFI_CONNECTING,  RGBColors::BLUE,       RGBEffect::PULSE,      0,    64},
    {SystemState::WIFI_FAILED,      RGBColors::RED,        RGBEffect::BLINK_SLOW, 0,    64},
    {SystemState::WEATHER_FETCHING, RGBColors::GREEN,      RGBEffect::PULSE,      0,    48},
    {SystemState::SD_READING,       RGBColors::ORANGE,     RGBEffect::PULSE,      0,    48},
    {SystemState::SD_WRITING,       RGBColors::YELLOW,     RGBEffect::PULSE,      0,    48},
    {SystemState::GOOGLE_DRIVE,     RGBColors::CYAN,       RGBEffect::PULSE,      0,    64},
    {SystemState::DOWNLOADING,      RGBColors::PURPLE,     RGBEffect::PULSE,      0,    64},
    {SystemState::RENDERING,        RGBColors::PINK,       RGBEffect::PULSE,      0,    48},
    {SystemState::BATTERY_LOW,      RGBColors::RED,        RGBEffect::BLINK_SLOW, 0,    48},
    {SystemState::ERROR,
     RGBColors::RED,
     RGBEffect::BLINK_FAST,
     0,                                                                                 96}, // Keep error reasonably bright for visibility
    {SystemState::SLEEP_PREP,       RGBColors::DIM_WHITE,  RGBEffect::FADE_OUT,   2000, 64},
    {SystemState::CUSTOM,           RGBColors::WHITE,      RGBEffect::SOLID,      0,    64}
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
    log_i("[RGB] Enabling LED power on GPIO%d", LED_PWR_PIN);
    pinMode(LED_PWR_PIN, OUTPUT);
    digitalWrite(LED_PWR_PIN, HIGH); // Power on
    delay(50); // Wait for power stabilization
#endif

    // Initialize NeoPixel
    pixels = new Adafruit_NeoPixel(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
    if (!pixels) {
        log_e("[RGB] Failed to create NeoPixel object");
#ifdef LED_PWR_PIN
        digitalWrite(LED_PWR_PIN, LOW); // Power off on failure
#endif
        return false;
    }

    delay(10); // Allow additional stabilization

    pixels->begin();
    pixels->clear();
    pixels->show();

    log_i("[RGB] NeoPixel initialized");

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
        log_e("[RGB] Failed to create RGB task");
        delete pixels;
        pixels = nullptr;
        return false;
    }

    taskRunning = true;
    log_i("[RGB] RGB status task started");

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
    log_i("[RGB] LED power disabled");
#endif

    log_i("[RGB] RGB status system stopped");
}

void RGBStatus::setState(SystemState state, uint16_t duration_ms) {
    if (!enabled || !pixels)
        return;

    // Find configuration for this state
    const StatusConfig* config = nullptr;
    for (size_t i = 0; i < NUM_STATUS_CONFIGS; i++) {
        if (STATUS_CONFIGS[i].state == state) {
            config = &STATUS_CONFIGS[i];
            break;
        }
    }

    if (!config) {
        log_w("[RGB] Warning: No configuration found for state %d", (int)state);
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

    log_i("[RGB] State changed to: %d", (int)state);
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

    log_i("[RGB] Custom color set: (%d,%d,%d) Effect: %d", color.r, color.g, color.b, (int)effect);
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
    log_i("[RGB] LED power disabled");
#endif

    currentState  = SystemState::IDLE;
    currentConfig = StatusConfig(SystemState::IDLE, RGBColors::OFF, RGBEffect::OFF);
}

// Static FreeRTOS task function
void RGBStatus::rgbTask(void* parameter) {
    RGBStatus* rgb = static_cast<RGBStatus*>(parameter);

    log_i("[RGB] RGB task started");

    while (rgb->taskRunning) {
        if (rgb->enabled && rgb->pixels) {
            rgb->updateEffect();
        }

        // Task runs at ~50Hz for smooth effects
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    log_i("[RGB] RGB task ended");
    vTaskDelete(NULL);
}

void RGBStatus::updateEffect() {
    unsigned long now = millis();

    // Check if effect duration has expired
    if (currentConfig.duration_ms > 0 && (now - lastUpdate) >= currentConfig.duration_ms) {
        setState(SystemState::IDLE);
        return;
    }

    // Always use pulsing rainbow effect regardless of state
    // This creates a continuous, smooth rainbow that pulses gently

    // Get rainbow color based on current position
    static uint16_t rainbowStep = 0;
    RGBColor rainbowColor = rainbow(rainbowStep / 4); // Slow down rainbow transition

    // Calculate pulse effect (breathing animation)
    uint8_t pulseBrightness = calculatePulse(effectStep, 120); // Slower pulse period

    // More pronounced pulsing: brightness varies from very dim to bright
    // Minimum brightness: ~10% (25/255)
    // Maximum brightness: ~25% (64/255) for power efficiency
    uint8_t minBrightness = 25;
    uint8_t maxBrightness = 64;

    // Map pulse (0-255) to brightness range (minBrightness to maxBrightness)
    uint8_t finalBrightness = minBrightness + ((maxBrightness - minBrightness) * pulseBrightness) / 255;

    // Set the pixel color with pulsing rainbow
    setPixelColor(rainbowColor, finalBrightness);

    // Update animation steps
    effectStep = (effectStep + 1) % 120;  // Pulse cycle
    rainbowStep = (rainbowStep + 1) % 1024;  // Rainbow cycle (slower with /4)
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