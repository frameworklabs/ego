// ego_motion
//
// Copyright (c) 2022, Framework Labs.

#include <AtomMotion.h>

#include <ego_common.h>

#include <pa_plankton.h>
#include <pa_atom.h>
#include <pa_utils.h>
#include <proto_activities.h>
#include <plankton.h>

#include <M5Atom.h>
#include <FastLED.h>

// Common Types

struct Speed {
    int8_t x;
    int8_t y;
};

// Intent

static auto redBtn = Button{19, true, 10};
static auto blueBtn = Button{22, true, 10};

pa_activity (IntentComputer, pa_ctx(), Press press, Press redPress, Press bluePress, Press rcPress, Intent& intent) {
    pa_always {
        switch (bluePress) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::START_MANU; break;
            case Press::LONG: break;
            case Press::DOUBLE: intent = Intent::START_AUTO; break;
        }
        switch (press) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::STOP; break;
            case Press::LONG: intent = Intent::QUIT; break;
            case Press::DOUBLE: intent = Intent::START_AUTO; break;
        }
        switch (redPress) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::STOP; break;
            case Press::LONG: intent = Intent::QUIT; break;
            case Press::DOUBLE: break;
        }
        switch (rcPress) {
            case Press::NO: break;
            case Press::SHORT: 
                if (intent == Intent::STOP) {
                    intent = Intent::START_MANU;
                } else {
                    intent = Intent::STOP; 
                }
                break;
            case Press::LONG: intent = Intent::QUIT; break;
            case Press::DOUBLE: intent = Intent::START_AUTO; break;
        }
    } pa_always_end;
} pa_end;

pa_activity (PressSubscriber, pa_ctx(), Press& press) {
    plankton.subscribe(Topic::PRESS, {});
    pa_always {
        plankton.read(Topic::PRESS, (uint8_t*)&press, 1);
    } pa_always_end;
} pa_end;

pa_activity (IntentRecognizer, 
            pa_ctx(pa_co_res(8); pa_use(ButtonUpdater);
                   pa_use_as(PressRecognizer, Main); 
                   pa_use_as(PressRecognizer, Red);
                   pa_use_as(PressRecognizer, Blue);
                   pa_use(PressSubscriber);  pa_use(IntentComputer); 
                   Press press; Press redPress; Press bluePress; Press rcPress), 
            Intent& intent) {
    pa_co(7) {
        pa_with (ButtonUpdater, redBtn);
        pa_with (ButtonUpdater, blueBtn);
        pa_with_as (PressRecognizer, Main, M5.Btn, pa_self.press);
        pa_with_as (PressRecognizer, Red, redBtn, pa_self.redPress);
        pa_with_as (PressRecognizer, Blue, blueBtn, pa_self.bluePress);
        pa_with (PressSubscriber, pa_self.rcPress);
        pa_with (IntentComputer, pa_self.press, pa_self.redPress, pa_self.bluePress, pa_self.rcPress, intent);
    } pa_co_end;
} pa_end;

pa_activity (IntentPublisher, pa_ctx(Intent prevIntent), Intent intent) {
    while (true) {
        plankton.publish(Topic::INTENT, (const uint8_t*)&intent, 1);
        pa_self.prevIntent = intent;
        pa_await (intent != pa_self.prevIntent);
    }
} pa_end;

// Blinker and Lights

enum TrafficLED : uint8_t {
    FRONT_RIGHT = 0,
    FRONT_CENTER = 1,
    FRONT_LEFT = 2,
    BACK_LEFT = 3,
    BACK_CENTER = 4,
    BACK_RIGHT = 5,
    NUM_TRAFFIC_LEDS,
};

enum class LatDir : uint8_t {
    CENTER,
    LEFT,
    RIGHT,
};

enum class LongDir : uint8_t {
    STOP,
    FORWARD,
    BACKWARD,
};

static CRGB trafficLEDs[NUM_TRAFFIC_LEDS];

pa_activity (BlinkerAux, pa_ctx(pa_use(Delay)), LatDir latDir) {
    if (latDir == LatDir::LEFT) {
        while (true) {
            trafficLEDs[FRONT_LEFT] = trafficLEDs[BACK_LEFT] = CRGB::Yellow;
            FastLED.show();
            pa_run (Delay, 5);

            trafficLEDs[FRONT_LEFT] = trafficLEDs[BACK_LEFT] = CRGB::Black;
            FastLED.show();
            pa_run (Delay, 5);
        }
    } else if (latDir == LatDir::RIGHT) {
        while (true) {
            trafficLEDs[FRONT_RIGHT] = trafficLEDs[BACK_RIGHT] = CRGB::Yellow;
            FastLED.show();
            pa_run (Delay, 5);

            trafficLEDs[FRONT_RIGHT] = trafficLEDs[BACK_RIGHT] = CRGB::Black;
            FastLED.show();
            pa_run (Delay, 5);
        }
    } else {
        pa_halt;
    }
} pa_end;

static void stopBlinker() {
    trafficLEDs[FRONT_LEFT] = trafficLEDs[BACK_LEFT] = CRGB::Black;
    trafficLEDs[FRONT_RIGHT] = trafficLEDs[BACK_RIGHT] = CRGB::Black;
    FastLED.show();
}

pa_activity (Blinker, pa_ctx(pa_use(BlinkerAux); LatDir latDirPrev), LatDir latDir) {
    while (true) {
        pa_self.latDirPrev = latDir;
        pa_when_abort (latDir != pa_self.latDirPrev, BlinkerAux, latDir);
        stopBlinker();
    }
} pa_end;

pa_activity (MainLightsAux, pa_ctx(pa_use(Delay)), LongDir dir) {
    if (dir == LongDir::FORWARD) {
        trafficLEDs[FRONT_CENTER] = CRGB::White;
        trafficLEDs[BACK_CENTER] = CRGB::Red;
        FastLED.show();
        pa_halt;
    } else if (dir == LongDir::BACKWARD) {
        trafficLEDs[FRONT_CENTER] = CRGB::White;

        while (true) {
            trafficLEDs[BACK_CENTER] = CRGB::Red;
            FastLED.show();
            pa_run (Delay, 5);

            trafficLEDs[BACK_CENTER] = CRGB::Black;
            FastLED.show();
            pa_run (Delay, 5);
        }
    } else {
        pa_halt;
    }
} pa_end;

static void stopMainLights() {    
    trafficLEDs[FRONT_CENTER] = CRGB::Black;
    trafficLEDs[BACK_CENTER] = CRGB::Black;
    FastLED.show();
}

pa_activity (MainLights, pa_ctx(LongDir dirPrev; pa_use(MainLightsAux)), LongDir dir) {
    while (true) {
        pa_self.dirPrev = dir;
        pa_when_abort (dir != pa_self.dirPrev, MainLightsAux, dir);
        stopMainLights();
    }
} pa_end;

pa_activity (DirGenerator, pa_ctx(), Speed speed, LongDir& longDir, LatDir& latDir) {
    pa_always {
        if (speed.y > 10) {
            longDir = LongDir::FORWARD;
        } else if (speed.y < -10) {
            longDir = LongDir::BACKWARD;
        } else {
            longDir = LongDir::STOP;
        }
        if (speed.x > 20) {
            latDir = LatDir::RIGHT;
        } else if (speed.x < -20) {
            latDir = LatDir::LEFT;
        } else {
            latDir = LatDir::CENTER;
        }
    } pa_always_end;
} pa_end;

pa_activity (Lights, pa_ctx(pa_co_res(4); LongDir longDir; LatDir latDir; pa_use(DirGenerator); pa_use(Blinker); pa_use(MainLights)), 
                     Speed speed) {
    pa_co(3) {
        pa_with (DirGenerator, speed, pa_self.longDir, pa_self.latDir);
        pa_with (Blinker, pa_self.latDir);
        pa_with (MainLights, pa_self.longDir);
    } pa_co_end;
} pa_end;

static void initLights() {
    FastLED.addLeds<NEOPIXEL, 26>(trafficLEDs, NUM_TRAFFIC_LEDS);
}

static void stopLights() {
    stopMainLights();
    stopBlinker();
}

// Actuator

static auto motion = AtomMotion();

static uint16_t invertPulse(uint16_t pulse) {
    const auto basePulse = pulse - 1500;
    return 1500 - basePulse;
}

pa_activity (PulseCalculator, pa_ctx(), Speed speed, uint16_t& leftPulse, uint16_t& rightPulse) {
    pa_always { 
        const uint16_t pulse = 1500 + speed.y * 4; 
        int16_t potFact = TARA + speed.x * 2;
        if (speed.y < -10) {
            potFact = -potFact;
        }
        leftPulse = pulse + potFact;
        rightPulse = invertPulse(pulse - potFact);
    } pa_always_end;
} pa_end;

static uint16_t guard(uint16_t pulse) {
    return min(uint16_t{2500}, max(uint16_t{500}, pulse));
}

pa_activity (Servo, pa_ctx(uint16_t leftPulsePrev; uint16_t rightPulsePrev), uint16_t leftPulse, uint16_t rightPulse) {
    while (true) {
        motion.SetServoPulse(1, guard(leftPulse));
        motion.SetServoPulse(3, guard(rightPulse));

        pa_self.leftPulsePrev = leftPulse;
        pa_self.rightPulsePrev = rightPulse;

        pa_await (leftPulse != pa_self.leftPulsePrev || rightPulse != pa_self.rightPulsePrev);
    }
} pa_end;

pa_activity (Actuator, pa_ctx(pa_co_res(2); pa_use(PulseCalculator); pa_use(Servo); uint16_t leftPulse; uint16_t rightPulse), Speed speed) {
    pa_co(2) {
        pa_with (PulseCalculator, speed, pa_self.leftPulse, pa_self.rightPulse);
        pa_with (Servo, pa_self.leftPulse, pa_self.rightPulse);
    } pa_co_end;
} pa_end;

static void stopActuator() {
    motion.SetServoPulse(1, 1500);
    motion.SetServoPulse(3, 1500);
}

// Driving

pa_activity (DriveForward, pa_ctx(), Speed& speed) {
    speed.x = 0;
    speed.y = 80;
    pa_halt;
} pa_end;

pa_activity (ToggleAfter, pa_ctx(pa_use(Delay)), unsigned ticks, bool& value) {
    pa_run (Delay, ticks);
    value = !value;
} pa_end;

pa_activity (DriveForwardAndSetRot, pa_ctx(pa_co_res(2); pa_use(DriveForward); pa_use(ToggleAfter)), Speed& speed, bool& rotClockwise) {
    pa_co(2) {
        pa_with (DriveForward, speed);
        pa_with_weak (ToggleAfter, 50, rotClockwise);
    } pa_co_end;
} pa_end;

pa_activity (Rotate, pa_ctx(), bool clockwise, Speed& speed) {
    speed.x = clockwise ? 100 : -100;
    speed.y = 0;
    pa_halt;
} pa_end;

pa_activity (RunAuto, pa_ctx(pa_use(DriveForwardAndSetRot); pa_use(Rotate); bool rotClockwise), uint16_t range, Speed& speed) {
    setLED(CRGB::Blue);
    while (true) {
        if (range > 300) {
            pa_when_abort (range <= 300, DriveForwardAndSetRot, speed, pa_self.rotClockwise);
        }
        pa_when_abort (range > 300, Rotate, pa_self.rotClockwise, speed);
    }
} pa_end;

pa_activity (RunManual, pa_ctx(), Speed joySpeed, Speed& speed) {
    setLED(CRGB::Green);
    pa_always {
        speed = joySpeed;
    } pa_always_end;
} pa_end;

pa_activity (Run, pa_ctx(pa_use(RunAuto); pa_use(RunManual)), Intent intent, Speed joySpeed, uint16_t range, Speed& speed) {
    if (intent == Intent::START_MANU) {
        pa_when_abort (intent != Intent::START_MANU || (joySpeed.y > 10 && range < 80), RunManual, joySpeed, speed);
    } else {
        pa_when_abort (intent != Intent::START_AUTO, RunAuto, range, speed);
    }
} pa_end;

// Controller

pa_activity (RangeSubscriber, pa_ctx(), uint16_t& range) {
    plankton.subscribe(Topic::RANGE, {});
    pa_always {
        plankton.read(Topic::RANGE, (uint8_t*)&range, 2);
    } pa_always_end;
} pa_end;

pa_activity (JoystickSubscriber, pa_ctx(), Speed& speed) {
    plankton.subscribe(Topic::JOYSTICK, {});
    pa_always {
        uint8_t buf[3];
        plankton.read(Topic::JOYSTICK, buf, 3);
        speed.x = buf[0];
        speed.y = buf[1];
    } pa_always_end;
} pa_end;

pa_activity (Logger, pa_ctx(), Speed speed, uint16_t range) {
    pa_always {
        Serial.printf("speed x: %u, y: %u\n", speed.x, speed.y);
        Serial.printf("range: %u\n", range);
    } pa_always_end;
} pa_end;

pa_activity (Controller, pa_ctx(pa_co_res(6); uint16_t range; Speed speed;
                             Speed joySpeed; pa_use(JoystickSubscriber);
                             pa_use(Run); pa_use(BlinkLED); pa_use(Logger);
                             pa_use(RangeSubscriber); pa_use(Actuator); pa_use(Lights)), 
                      Intent intent) {
    setLED(CRGB::Red);

    while (true) {
        pa_await (intent == Intent::START_AUTO || intent == Intent::START_MANU || intent == Intent::QUIT);

        if (intent == Intent::QUIT) {
            break;
        }
        pa_co(6) {
            pa_with_weak (JoystickSubscriber, pa_self.joySpeed);
            pa_with_weak (RangeSubscriber, pa_self.range);
            pa_with (Run, intent, pa_self.joySpeed, pa_self.range, pa_self.speed);
            pa_with_weak (Actuator, pa_self.speed);
            pa_with_weak (Lights, pa_self.speed);
            pa_with_weak (Logger, pa_self.speed, pa_self.range);
        } pa_co_end;

        stopActuator();
        stopLights();
        setLED(CRGB::Red);
    }
} pa_end;

// Main

pa_activity (Main, pa_ctx(pa_co_res(4); Intent intent; pa_use(IntentPublisher);
                          pa_use(IntentRecognizer); pa_use(BlinkLED); pa_use(Receiver);                          
                          pa_use(Controller); pa_use(Connector))) {
    Serial.println("Start");
    pa_co(2) {
        pa_with (Connector);
        pa_with_weak (BlinkLED, CRGB::Orange, 5, 5);
    } pa_co_end;
    clearLED();

    pa_co(4) {
        pa_with_weak (Receiver);
        pa_with_weak (IntentRecognizer, pa_self.intent);
        pa_with_weak (IntentPublisher, pa_self.intent);
        pa_with (Controller, pa_self.intent);
    } pa_co_end;
    
    Serial.println("Done");
    pa_run (BlinkLED, CRGB::Red, 10, 5);
} pa_end;

// Setup and Loop

static pa_use(Main);

void setup() {
    setCpuFrequencyMhz(80);

    M5.begin();

    motion.Init();

    initLights();
    initLED();
}

void loop() {
    TickType_t prevWakeTime = xTaskGetTickCount();

    while (true) {
        M5.update();

        pa_tick(Main);

        // We run at 10 Hz.
        vTaskDelayUntil(&prevWakeTime, 100);
    }
}
