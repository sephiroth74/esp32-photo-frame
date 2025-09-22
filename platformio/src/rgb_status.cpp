#include "rgb_status.h"
#include "config.h"

// Global RGB status instance
RGBStatus rgbStatus;

// Predefined status configurations - optimized for power efficiency (max brightness 64)
const StatusConfig RGBStatus::STATUS_CONFIGS[] = {
    {SystemState::IDLE,            RGBColors::DARK_BLUE,   RGBEffect::SOLID,      0,    24},
    {SystemState::STARTING,        RGBColors::WHITE,       RGBEffect::PULSE,      3000, 64},
    {SystemState::WIFI_CONNECTING, RGBColors::BLUE,        RGBEffect::PULSE,      0,    64},
    {SystemState::WIFI_FAILED,     RGBColors::RED,         RGBEffect::BLINK_SLOW, 0,    64},
    {SystemState::WEATHER_FETCHING,RGBColors::GREEN,       RGBEffect::PULSE,      0,    48},
    {SystemState::SD_READING,      RGBColors::ORANGE,      RGBEffect::PULSE,      0,    48},
    {SystemState::SD_WRITING,      RGBColors::YELLOW,      RGBEffect::PULSE,      0,    48},
    {SystemState::GOOGLE_DRIVE,    RGBColors::CYAN,        RGBEffect::PULSE,      0,    64},
    {SystemState::DOWNLOADING,     RGBColors::PURPLE,      RGBEffect::PULSE,      0,    64},
    {SystemState::RENDERING,       RGBColors::PINK,        RGBEffect::PULSE,      0,    48},
    {SystemState::BATTERY_LOW,     RGBColors::RED,         RGBEffect::BLINK_SLOW, 0,    48},
    {SystemState::ERROR,           RGBColors::RED,         RGBEffect::BLINK_FAST, 0,    96}, // Keep error reasonably bright for visibility
    {SystemState::SLEEP_PREP,      RGBColors::DIM_WHITE,   RGBEffect::FADE_OUT,   2000, 64},
    {SystemState::CUSTOM,          RGBColors::WHITE,       RGBEffect::SOLID,      0,    64}
};

const size_t RGBStatus::NUM_STATUS_CONFIGS = sizeof(STATUS_CONFIGS) / sizeof(StatusConfig);

RGBStatus::RGBStatus()
    : pixels(nullptr)
    , rgbTaskHandle(nullptr)
    , currentState(SystemState::IDLE)
    , currentConfig(SystemState::IDLE, RGBColors::OFF)
    , enabled(true)
    , taskRunning(false)
    , lastUpdate(0)
    , effectStep(0)
    , currentBrightness(0)
{
}

RGBStatus::~RGBStatus() {
    end();
}

bool RGBStatus::begin() {
    // Initialize NeoPixel
    pixels = new Adafruit_NeoPixel(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
    if (!pixels) {
        Serial.println(F("[RGB] Failed to create NeoPixel object"));
        return false;
    }

    // Power on the NeoPixel (FeatherS3 has power control on GPIO39)
    pinMode(RGB_LED_PWR_PIN, OUTPUT);
    digitalWrite(RGB_LED_PWR_PIN, HIGH);
    delay(10); // Allow power to stabilize

    pixels->begin();
    pixels->clear();
    pixels->show();

    Serial.println(F("[RGB] NeoPixel initialized"));

    // Create FreeRTOS task for RGB control
    BaseType_t result = xTaskCreatePinnedToCore(
        rgbTask,           // Task function
        "RGBStatusTask",   // Task name
        2048,              // Stack size (bytes)
        this,              // Task parameter (this instance)
        1,                 // Priority (1 = low priority)
        &rgbTaskHandle,    // Task handle
        0                  // Core ID (0 = any core)
    );

    if (result != pdPASS) {
        Serial.println(F("[RGB] Failed to create RGB task"));
        delete pixels;
        pixels = nullptr;
        return false;
    }

    taskRunning = true;
    Serial.println(F("[RGB] RGB status task started"));

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

    // Power off NeoPixel
    digitalWrite(RGB_LED_PWR_PIN, LOW);

    Serial.println(F("[RGB] RGB status system stopped"));
}

void RGBStatus::setState(SystemState state, uint16_t duration_ms) {
    if (!enabled || !pixels) return;

    // Find configuration for this state
    const StatusConfig* config = nullptr;
    for (size_t i = 0; i < NUM_STATUS_CONFIGS; i++) {
        if (STATUS_CONFIGS[i].state == state) {
            config = &STATUS_CONFIGS[i];
            break;
        }
    }

    if (!config) {
        Serial.print(F("[RGB] Warning: No configuration found for state "));
        Serial.println((int)state);
        return;
    }

    currentState = state;
    currentConfig = *config;
    if (duration_ms > 0) {
        currentConfig.duration_ms = duration_ms;
    }

    // Reset effect timing
    lastUpdate = millis();
    effectStep = 0;

    Serial.print(F("[RGB] State changed to: "));
    Serial.print((int)state);
    Serial.print(F(" ("));
    Serial.print(currentConfig.color.r);
    Serial.print(F(","));
    Serial.print(currentConfig.color.g);
    Serial.print(F(","));
    Serial.print(currentConfig.color.b);
    Serial.print(F(") Effect: "));
    Serial.println((int)currentConfig.effect);
}

void RGBStatus::setCustomColor(const RGBColor& color, RGBEffect effect,
                              uint16_t duration_ms, uint8_t brightness) {
    if (!enabled || !pixels) return;

    currentState = SystemState::CUSTOM;
    currentConfig = StatusConfig(SystemState::CUSTOM, color, effect, duration_ms, brightness);

    // Reset effect timing
    lastUpdate = millis();
    effectStep = 0;

    Serial.print(F("[RGB] Custom color set: ("));
    Serial.print(color.r);
    Serial.print(F(","));
    Serial.print(color.g);
    Serial.print(F(","));
    Serial.print(color.b);
    Serial.print(F(") Effect: "));
    Serial.println((int)effect);
}

void RGBStatus::setBrightness(uint8_t brightness) {
    currentConfig.brightness = brightness;
}

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
    currentState = SystemState::IDLE;
    currentConfig = StatusConfig(SystemState::IDLE, RGBColors::OFF, RGBEffect::OFF);
}

// Static FreeRTOS task function
void RGBStatus::rgbTask(void* parameter) {
    RGBStatus* rgb = static_cast<RGBStatus*>(parameter);

    Serial.println(F("[RGB] RGB task started"));

    while (rgb->taskRunning) {
        if (rgb->enabled && rgb->pixels) {
            rgb->updateEffect();
        }

        // Task runs at ~50Hz for smooth effects
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    Serial.println(F("[RGB] RGB task ended"));
    vTaskDelete(NULL);
}

void RGBStatus::updateEffect() {
    unsigned long now = millis();

    // Check if effect duration has expired
    if (currentConfig.duration_ms > 0 && (now - lastUpdate) >= currentConfig.duration_ms) {
        setState(SystemState::IDLE);
        return;
    }

    uint8_t brightness = currentConfig.brightness;
    RGBColor color = currentConfig.color;

    switch (currentConfig.effect) {
        case RGBEffect::SOLID:
            setPixelColor(color, brightness);
            break;

        case RGBEffect::PULSE: {
            uint8_t pulseBrightness = calculatePulse(effectStep, 100);
            brightness = (brightness * pulseBrightness) / 255;
            setPixelColor(color, brightness);
            effectStep = (effectStep + 1) % 100;
            break;
        }

        case RGBEffect::BLINK_SLOW:
            if ((effectStep / 25) % 2 == 0) {
                setPixelColor(color, brightness);
            } else {
                setPixelColor(RGBColors::OFF, 0);
            }
            effectStep = (effectStep + 1) % 100;
            break;

        case RGBEffect::BLINK_FAST:
            if ((effectStep / 10) % 2 == 0) {
                setPixelColor(color, brightness);
            } else {
                setPixelColor(RGBColors::OFF, 0);
            }
            effectStep = (effectStep + 1) % 40;
            break;

        case RGBEffect::FADE_IN:
            brightness = (brightness * effectStep) / 100;
            setPixelColor(color, brightness);
            if (effectStep < 100) {
                effectStep++;
            }
            break;

        case RGBEffect::FADE_OUT:
            brightness = (brightness * (100 - effectStep)) / 100;
            setPixelColor(color, brightness);
            if (effectStep < 100) {
                effectStep++;
            } else {
                turnOff();
            }
            break;

        case RGBEffect::RAINBOW: {
            RGBColor rainbowColor = rainbow(effectStep);
            brightness = currentConfig.brightness;
            setPixelColor(rainbowColor, brightness);
            effectStep = (effectStep + 1) % 256;
            break;
        }

        case RGBEffect::OFF:
        default:
            setPixelColor(RGBColors::OFF, 0);
            break;
    }
}

void RGBStatus::setPixelColor(const RGBColor& color, uint8_t brightness) {
    if (!pixels) return;

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
    float sine = sin(angle);
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