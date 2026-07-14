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
constexpr uint32_t TOUCH_MULTI_CLICK_MS = 350;
constexpr uint32_t UNPAIRED_BLINK_INTERVAL_MS = 500;
constexpr uint32_t RESULT_LED_DURATION_MS = 400;
constexpr uint32_t VIBRATION_PULSE_MS = 70;
constexpr uint32_t VIBRATION_GAP_MS = 70;

SeeedHADiscovery ha;
IRMateInfrared infrared(ha, IR_TRANSMIT_PIN, IR_RECEIVE_PIN);
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

bool touchRawState = false;
bool touchStableState = false;
uint32_t touchChangedAt = 0;
uint32_t lastTouchAt = 0;
uint8_t touchCount = 0;
uint32_t feedbackEndsAt = 0;
uint32_t vibrationPhaseEndsAt = 0;
uint32_t lastUnpairedBlinkAt = 0;
uint32_t currentColor = UINT32_MAX;
uint8_t vibrationPulsesRemaining = 0;
bool vibrationMotorOn = false;
bool unpairedBlinkOn = false;

void setStatusColor(uint32_t color) {
    if (currentColor == color) {
        return;
    }
    currentColor = color;
    statusLed.setPixelColor(0, color);
    statusLed.show();
}

void startVibrationPattern(uint8_t pulseCount) {
    vibrationPulsesRemaining = pulseCount;
    vibrationMotorOn = pulseCount > 0;
    digitalWrite(VIBRATION_PIN, vibrationMotorOn ? HIGH : LOW);
    vibrationPhaseEndsAt = vibrationMotorOn ? millis() + VIBRATION_PULSE_MS : 0;
}

/**
 * Advance the vibration pulse sequence without blocking the main loop
 * 在不阻塞主循环的情况下推进震动脉冲序列
 */
void updateVibrationPattern(uint32_t now) {
    if (vibrationPhaseEndsAt == 0 ||
        static_cast<int32_t>(now - vibrationPhaseEndsAt) < 0) {
        return;
    }

    if (vibrationMotorOn) {
        vibrationMotorOn = false;
        digitalWrite(VIBRATION_PIN, LOW);
        vibrationPulsesRemaining--;
        vibrationPhaseEndsAt = vibrationPulsesRemaining > 0
            ? now + VIBRATION_GAP_MS
            : 0;
        return;
    }

    vibrationMotorOn = true;
    digitalWrite(VIBRATION_PIN, HIGH);
    vibrationPhaseEndsAt = now + VIBRATION_PULSE_MS;
}

void startResultFeedback(bool success) {
    feedbackEndsAt = millis() + RESULT_LED_DURATION_MS;
    setStatusColor(
        success ? statusLed.Color(0, 255, 0) : statusLed.Color(255, 0, 0)
    );
    startVibrationPattern(success ? 1 : 2);
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

    if (infrared.isLearning()) {
        return;
    }

    if (touchCount < UINT8_MAX) {
        touchCount++;
    }
    lastTouchAt = now;
}

void handleTouchAction() {
    if (touchCount == 0 || millis() - lastTouchAt < TOUCH_MULTI_CLICK_MS) {
        return;
    }

    uint8_t completedCount = touchCount;
    touchCount = 0;

    bool sent = false;
    if (completedCount == 1) {
        Serial.println("Touch action: toggle Gree power");
        sent = infrared.toggleDefaultGreePower();
    } else if (completedCount == 2) {
        Serial.println("Touch action: increase Gree temperature");
        sent = infrared.increaseDefaultGreeTemperature();
    } else if (completedCount == 3) {
        Serial.println("Touch action: decrease Gree temperature");
        sent = infrared.decreaseDefaultGreeTemperature();
    } else {
        Serial.printf("Touch action ignored: %u touches\n", completedCount);
    }

    if (!sent && completedCount <= 3) {
        startResultFeedback(false);
    }
}

void updateStatus() {
    uint32_t now = millis();
    updateVibrationPattern(now);

    if (feedbackEndsAt != 0) {
        if (static_cast<int32_t>(now - feedbackEndsAt) < 0) {
            return;
        }
        feedbackEndsAt = 0;
    }

    if (ha.isHAConnected()) {
        setStatusColor(0);
        return;
    }

    if (now - lastUnpairedBlinkAt >= UNPAIRED_BLINK_INTERVAL_MS) {
        lastUnpairedBlinkAt = now;
        unpairedBlinkOn = !unpairedBlinkOn;
        setStatusColor(unpairedBlinkOn ? statusLed.Color(0, 0, 255) : 0);
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
    });
    infrared.onTransmitCompleted([](bool success) {
        Serial.printf("IR transmission: %s\n", success ? "success" : "failed");
        startResultFeedback(success);
    });
    infrared.onLearningStateChanged([](bool active) {
        if (active) {
            touchCount = 0;
        }
        Serial.printf("IR learning: %s\n", active ? "started" : "stopped");
    });
    infrared.onLearningCompleted([](bool success) {
        Serial.printf("IR learning result: %s\n", success ? "success" : "failed");
        startResultFeedback(success);
    });
    infrared.begin();
    Serial.println("Offline Gree control ready");

    ha.beginWithProvisioning("Seeed_IR_Mate");
    ha.enableResetButton(RESET_PIN, true);
}

void loop() {
    ha.handle();
    infrared.handle();
    handleTouchButton();
    handleTouchAction();
    updateStatus();
    delay(2);
}
