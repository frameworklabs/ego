// ego_ranger
//
// Copyright (c) 2022, Framework Labs.

#include <ego_common.h>

#include <pa_ranging.h>
#include <pa_plankton.h>
#include <pa_atom.h>
#include <pa_utils.h>
#include <proto_activities.h>
#include <plankton.h>

#include <M5Atom.h>

// Ranging Helpers

enum class IndicatorLevel : uint8_t {
    UNDEF = 0,
    NEAR,
    MEDIUM,
    FAR
};

static IndicatorLevel calcLevel(uint16_t range) {
    if (range > 1000) {
        return IndicatorLevel::FAR;
    }
    else if (range > 300) {
        return IndicatorLevel::MEDIUM;
    }
    else {
        return IndicatorLevel::NEAR;
    }
}

pa_activity (RangeIndicator, pa_ctx(IndicatorLevel prevLevel), uint16_t range) {
    pa_always {
        const auto level = calcLevel(range);
        if (level != pa_self.prevLevel) {
            switch (level) {
                case IndicatorLevel::FAR: setLED(CRGB::Green); break;
                case IndicatorLevel::MEDIUM: setLED(CRGB::Yellow); break;
                case IndicatorLevel::NEAR: setLED(CRGB::Red); break;
                case IndicatorLevel::UNDEF: setLED(CRGB::Black); break;
            }
            pa_self.prevLevel = level;
        }
    } pa_always_end;
} pa_end;

pa_activity (RangePublisher, pa_ctx(uint16_t prevRange), uint16_t range) {
    while (true) {
        Serial.printf("publishing range: %u\n", range);
        plankton.publish(Topic::RANGE, (const uint8_t*)&range, 2);
        pa_self.prevRange = range;
        pa_await (range != pa_self.prevRange);
    }
} pa_end;

// Top-Level Activities

pa_activity (RangeController, pa_ctx(uint16_t range; pa_co_res(4); pa_use(Ranger);
                                     pa_use(RangeIndicator); pa_use(RangePublisher))) {
    pa_co(3) {
        pa_with (Ranger, pa_self.range);
        pa_with (RangePublisher, pa_self.range);
        pa_with (RangeIndicator, pa_self.range);
    } pa_co_end;
} pa_end;

pa_activity (ModeController, pa_ctx(pa_use(RangeController); pa_use(BlinkLED)), Press press) {
    while (true) {
        pa_when_abort (press == Press::SHORT, RangeController);
        stopRanging();

        pa_when_abort (press == Press::SHORT, BlinkLED, CRGB::White, 5, 10);
    }
} pa_end;

pa_activity (Main, pa_ctx(pa_co_res(2); Press press;
                          pa_use(BlinkLED); pa_use(Connector); 
                          pa_use(PressRecognizer); pa_use(ModeController)), 
                   bool setupOK) {
    if (!setupOK) {
        pa_run (BlinkLED, CRGB::Red, 10, 10);
    }

    pa_co(2) {
        pa_with (Connector);
        pa_with_weak (BlinkLED, CRGB::Orange, 5, 5);
    } pa_co_end;
    clearLED();

    pa_co(2) {
        pa_with (PressRecognizer, M5.Btn, pa_self.press);
        pa_with (ModeController, pa_self.press);
    } pa_co_end;
} pa_end;

// Setup and Loop

static pa_use(Main);
static bool setupOK = false;

void setup() {
    setCpuFrequencyMhz(80);

    M5.begin();

    initLED();
    
    if (!initRanging(19, 22)) {
        return;
    }

    setupOK = true;
}

void loop() {
    TickType_t prevWakeTime = xTaskGetTickCount();

    while (true) {
        M5.update();

        pa_tick(Main, setupOK);

        // We run at 10 Hz.
        vTaskDelayUntil(&prevWakeTime, 100);
    }
}
