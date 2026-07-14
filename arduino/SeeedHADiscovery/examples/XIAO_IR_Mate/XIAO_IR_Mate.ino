/**
 * IR Mate universal remote example
 * IR Mate 万能红外遥控器示例
 */

#include <Adafruit_NeoPixel.h>
#include <SeeedHADiscovery.h>

#include "IRMateInfrared.h"

constexpr uint8_t IR_TRANSMIT_PIN = 3;
constexpr uint8_t IR_RECEIVE_PIN = 4;
constexpr uint8_t TOUCH_PIN = 5;
constexpr uint8_t VIBRATION_PIN = 6;
constexpr uint8_t STATUS_LED_PIN = 7;
constexpr uint8_t RESET_PIN = 9;

constexpr uint32_t TOUCH_DEBOUNCE_MS = 35;
constexpr uint32_t WIFI_BLINK_INTERVAL_MS = 500;

SeeedHADiscovery ha;
IRMateInfrared infrared(ha, IR_TRANSMIT_PIN, IR_RECEIVE_PIN);
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool touchRawState = false;
bool touchStableState = false;
uint32_t touchChangedAt = 0;
uint32_t feedbackEndsAt = 0;
uint32_t lastWifiBlinkAt = 0;
uint32_t currentColor = UINT32_MAX;
bool wifiBlinkOn = false;

void setStatusColor(uint32_t color) {
    if (currentColor == color) {
        return;
    }
    currentColor = color;
    statusLed.setPixelColor(0, color);
    statusLed.show();
}

void startFeedback(uint32_t color, uint32_t durationMs, bool vibrate) {
    feedbackEndsAt = millis() + durationMs;
    setStatusColor(color);
    digitalWrite(VIBRATION_PIN, vibrate ? HIGH : LOW);
}

void handleTouchButton() {
    bool rawState = digitalRead(TOUCH_PIN) == HIGH;
    uint32_t now = millis();

    if (rawState != touchRawState) {
        touchRawState = rawState;
        touchChangedAt = now;
    }

    if (now - touchChangedAt < TOUCH_DEBOUNCE_MS || rawState == touchStableState) {
        return;
    }

    touchStableState = rawState;
    if (!touchStableState) {
        return;
    }

    // A local press replays the latest learned signal without Home Assistant.
    // 本地按键可在不依赖 Home Assistant 的情况下重放最近信号。
    if (!infrared.replayLastSignal()) {
        startFeedback(statusLed.Color(255, 0, 0), 350, true);
    }
}

void updateStatus() {
    uint32_t now = millis();

    if (feedbackEndsAt != 0) {
        if (static_cast<int32_t>(now - feedbackEndsAt) < 0) {
            return;
        }
        feedbackEndsAt = 0;
        digitalWrite(VIBRATION_PIN, LOW);
    }

    if (ha.isHAConnected()) {
        setStatusColor(statusLed.Color(180, 180, 180));
        return;
    }

    if (now - lastWifiBlinkAt >= WIFI_BLINK_INTERVAL_MS) {
        lastWifiBlinkAt = now;
        wifiBlinkOn = !wifiBlinkOn;
        setStatusColor(wifiBlinkOn ? statusLed.Color(0, 0, 255) : 0);
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(TOUCH_PIN, INPUT_PULLDOWN);
    pinMode(VIBRATION_PIN, OUTPUT);
    digitalWrite(VIBRATION_PIN, LOW);

    statusLed.begin();
    statusLed.setBrightness(180);
    setStatusColor(statusLed.Color(0, 0, 255));

    ha.setDeviceInfo("IR Mate", "Seeed IR Mate", "1.0.0");
    ha.enableDebug(true);

    infrared.setDefaultCarrierFrequency(38000);
    infrared.onSignalReceived([](size_t timingCount) {
        Serial.printf(
            "IR signal received: %u timings\n",
            static_cast<unsigned int>(timingCount)
        );
        startFeedback(statusLed.Color(0, 255, 0), 180, true);
    });
    infrared.onTransmitCompleted([](bool success) {
        Serial.printf("IR transmission: %s\n", success ? "success" : "failed");
        startFeedback(
            success ? statusLed.Color(0, 180, 255) : statusLed.Color(255, 0, 0),
            180,
            true
        );
    });
    infrared.begin();

    ha.beginWithProvisioning("Seeed_IR_Mate");
    ha.enableResetButton(RESET_PIN, true);
}

void loop() {
    ha.handle();
    infrared.handle();
    handleTouchButton();
    updateStatus();
    delay(2);
}
