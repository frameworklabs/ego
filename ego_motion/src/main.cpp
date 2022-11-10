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
#include <NeoPixelBrightnessBus.h>

#include <algorithm>

// Common Types

struct Speed {
    int8_t x;
    int8_t y;
};

bool operator ==(Speed lhs, Speed rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator != (Speed lhs, Speed rhs) {
    return !(lhs == rhs);
}

// Intent

static auto redBtn = Button{19, true, 10}; // Port C (Blue) on Motion - Port B (Black) would be 33
static auto blueBtn = Button{22, true, 10}; // Port C (Blue) on Motion - Port B (Black) would be 23

pa_activity (IntentComputer, pa_ctx(), Press press, Press redPress, Press bluePress, Press rcPress, Intent& intent) {
    pa_always {
        bool considerRc = true;

        switch (bluePress) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::START_MANU; break;
            case Press::LONG: break;
            case Press::DOUBLE: intent = Intent::START_AUTO; break;
        }
        switch (press) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::STOP; considerRc = false; break;
            case Press::LONG: intent = Intent::QUIT; considerRc = false; break;
            case Press::DOUBLE: intent = Intent::START_AUTO; break;
        }
        switch (redPress) {
            case Press::NO: break;
            case Press::SHORT: intent = Intent::STOP; considerRc = false; break;
            case Press::LONG: intent = Intent::QUIT; considerRc = false; break;
            case Press::DOUBLE: break;
        }
        if (considerRc) {
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

static const RgbColor red(255, 0, 0);
static const RgbColor yellow(255, 255, 0);
static const RgbColor white(255, 255, 255);
static const RgbColor black(0, 0, 0);

static NeoPixelBrightnessBus<NeoGrbFeature, NeoSk6812Method> strip(NUM_TRAFFIC_LEDS, 33);
static bool needsShow = false;

static void initLights() {
    strip.Begin();
    strip.SetBrightness(5);
}

static void setNeedsShow() {
    needsShow = true;
}

static void showIfNeeded() {
    if (needsShow) {
        strip.Show();
        needsShow = false;
    }
}

pa_activity (BlinkPixel, pa_ctx(pa_use(Delay)), uint16_t pixel, RgbColor color, unsigned on, unsigned off) {
    while (true) {
        strip.SetPixelColor(pixel, color);
        setNeedsShow();
        pa_run (Delay, on);

        strip.SetPixelColor(pixel, black);
        setNeedsShow();
        pa_run (Delay, off);
    }
} pa_end;

pa_activity (BlinkerAux, pa_ctx(pa_co_res(2); pa_use_as(BlinkPixel, BlinkFront);  pa_use_as(BlinkPixel, BlinkBack)), 
                         LatDir latDir) {
    if (latDir == LatDir::LEFT) {
        pa_co(2) {
            pa_with_as (BlinkPixel, BlinkFront, FRONT_LEFT, yellow, 3, 6);
            pa_with_as (BlinkPixel, BlinkBack, BACK_LEFT, yellow, 3, 6);
        } pa_co_end;
    } else if (latDir == LatDir::RIGHT) {
        pa_co(2) {
            pa_with_as (BlinkPixel, BlinkFront, FRONT_RIGHT, yellow, 3, 6);
            pa_with_as (BlinkPixel, BlinkBack, BACK_RIGHT, yellow, 3, 6);
        } pa_co_end;
    } else {
        pa_halt;
    }
} pa_end;

static void stopBlinker() {
    strip.SetPixelColor(FRONT_LEFT, black);
    strip.SetPixelColor(BACK_LEFT, black);
    strip.SetPixelColor(FRONT_RIGHT, black);
    strip.SetPixelColor(BACK_RIGHT, black);
    setNeedsShow();
}

pa_activity (Blinker, pa_ctx(pa_use(BlinkerAux); LatDir latDirPrev), LatDir latDir) {
    while (true) {
        pa_self.latDirPrev = latDir;
        pa_when_abort (latDir != pa_self.latDirPrev, BlinkerAux, latDir);
        stopBlinker();
    }
} pa_end;

pa_activity (MainLightsAux, pa_ctx(pa_use(BlinkPixel)), LongDir dir) {
    if (dir == LongDir::FORWARD) {
        strip.SetPixelColor(FRONT_CENTER, white);
        strip.SetPixelColor(BACK_CENTER, red);
        setNeedsShow();
        pa_halt;
    } else if (dir == LongDir::BACKWARD) {
        strip.SetPixelColor(FRONT_CENTER, white);
        pa_run (BlinkPixel, BACK_CENTER, red, 5, 5);
    } else {
        pa_halt;
    }
} pa_end;

static void stopMainLights() {    
    strip.SetPixelColor(FRONT_CENTER, black);
    strip.SetPixelColor(BACK_CENTER, black);
    setNeedsShow();
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

static void stopLights() {
    stopMainLights();
    stopBlinker();
}

// Actuator

static auto motion = AtomMotion();
static constexpr auto motor = 3;
static constexpr auto steering = 4;

template <typename T>
T clamp(T v, T mi, T ma) {
    return min(max(v, mi), ma);
}

pa_activity (MotorServo, pa_ctx(int8_t prevSpeed), int8_t speed) {
    while (true) {
        {
            const uint16_t pulse = uint16_t{1500} + speed * 6;
            motion.SetServoPulse(motor, clamp(pulse, uint16_t{500}, uint16_t{2500}));
        }
        pa_self.prevSpeed = speed;
        pa_await (speed != pa_self.prevSpeed);
    }
} pa_end;

static constexpr int8_t STEERING_TARA = -10;

pa_activity (SteeringServo, pa_ctx(int8_t prevHeading), int8_t heading) {
    while (true) {
        {
            const uint8_t heading_deg = -heading / 127.0f * 45.0f;
            const uint8_t angle_deg = uint8_t{90 + STEERING_TARA} + uint8_t{heading_deg};
            motion.SetServoAngle(steering, clamp(angle_deg, uint8_t{45 + STEERING_TARA}, uint8_t{135 + STEERING_TARA}));
        }
        pa_self.prevHeading = heading;
        pa_await (heading != pa_self.prevHeading);
    }
} pa_end;

pa_activity (Actuator, pa_ctx(pa_co_res(2); pa_use(MotorServo); pa_use(SteeringServo)), Speed speed) {
    pa_co(2) {
        pa_with (MotorServo, speed.y);
        pa_with (SteeringServo, speed.x);
    } pa_co_end;
} pa_end;

static void stopActuator() {
    motion.SetServoPulse(motor, 1500);
    motion.SetServoAngle(steering, 90);
}

// Driving

static int8_t restrictChange(int8_t commandedVal, int8_t val, int8_t maxChange) {
    const auto delta = commandedVal - val;
    if (delta >= maxChange) {
        return val + maxChange;
    } else if (delta <= -maxChange) {
        return val - maxChange;
    } else {
        return val + delta;
    }
}

pa_activity (SpeedFilter, pa_ctx(), Speed commandedSpeed, Speed& speed) {
    pa_always {
        speed.x = restrictChange(commandedSpeed.x, speed.x, 127);
        speed.y = restrictChange(commandedSpeed.y, speed.y, 30);
    } pa_always_end;
} pa_end;

pa_activity (DriveForward, pa_ctx(), Speed& speed) {
    speed.x = 0;
    speed.y = 100;
    pa_halt;
} pa_end;

pa_activity (Step, pa_ctx(pa_use(Delay)), int8_t x, int8_t y, Speed& speed) {
    speed = {x, 0};
    pa_pause;
    speed = {x, y};
    pa_run (Delay, 10);
    speed = {0, 0};
    pa_pause;
} pa_end;

pa_activity (Rotate, pa_ctx(pa_use(Step); int8_t steps), uint16_t range, Speed& speed, bool& foundWay) {
    foundWay = false;
    for (pa_self.steps = 0; pa_self.steps < 10; ++pa_self.steps) {
        pa_run (Step, -127, -30, speed);
        if (range > 500) {
            foundWay = true;
            break;
        }
        pa_run (Step, 127, 30, speed);
        if (range > 500) {
            foundWay = true;
            break;
        }
    }
} pa_end;

pa_activity (RunAuto, pa_ctx(pa_use(DriveForward); pa_use(Rotate); pa_use(Delay); bool foundWay), 
                      uint16_t range, Speed& speed) {
    setLED(CRGB::Blue);

    while (true) {
        pa_when_abort (range <= 300, DriveForward, speed);
        speed.y = 0;
        pa_run (Rotate, range, speed, pa_self.foundWay);
        if (!pa_self.foundWay) {
            pa_halt;
        }
    }
} pa_end;

pa_activity (RunManual, pa_ctx(), Speed joySpeed, uint16_t range, Speed& speed) {
    setLED(CRGB::Green);

    pa_always {
        speed = joySpeed;
        if (joySpeed.y > 10 && range < 200) {
            speed.y = 0;
        }
    } pa_always_end;
} pa_end;

pa_activity (Run, pa_ctx(pa_use(RunAuto); pa_use(RunManual)), Intent intent, Speed joySpeed, uint16_t range, Speed& speed) {
    if (intent == Intent::START_MANU) {
        pa_when_abort (intent != Intent::START_MANU, RunManual, joySpeed, range, speed);
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

static int8_t clampJoy(int8_t val) {
    float res = clamp<int8_t>(val, -89, 89) * 127.0f / 89.0f;
    return (int8_t)res;
}

pa_activity (JoySpeedLimiter, pa_ctx(), Speed joySpeed, Speed& limitedJoySpeed) {
    pa_always {
        limitedJoySpeed.x = clampJoy(joySpeed.x);
        limitedJoySpeed.y = clampJoy(joySpeed.y);
    } pa_always_end;
} pa_end;

pa_activity (Logger, pa_ctx(pa_use(Delay)), Speed joySpeed, uint16_t range, Speed speed) {
    while (true) {
        Serial.printf("joy x: %d, y: %d\n", joySpeed.x, joySpeed.y);
        Serial.printf("range: %u\n", range);
        Serial.printf("speed x: %d, y: %d\n", speed.x, speed.y);
        pa_run (Delay, 10);
    };
} pa_end;

pa_activity (Controller, pa_ctx(pa_co_res(8); uint16_t range; Speed speed; Speed filteredSpeed;
                             Speed joySpeed; Speed limitedJoySpeed; pa_use(JoystickSubscriber);
                             pa_use(Run); pa_use(BlinkLED); pa_use(Logger); pa_use(JoySpeedLimiter);
                             pa_use(RangeSubscriber); pa_use(Actuator); pa_use(SpeedFilter);
                             pa_use(Lights)), 
                      Intent intent) {
    setLED(CRGB::Red);

    while (true) {
        pa_await (intent == Intent::START_AUTO || intent == Intent::START_MANU || intent == Intent::QUIT);

        if (intent == Intent::QUIT) {
            break;
        }
        pa_co(8) {
            pa_with_weak (JoystickSubscriber, pa_self.joySpeed);
            pa_with_weak (JoySpeedLimiter, pa_self.joySpeed, pa_self.limitedJoySpeed);
            pa_with_weak (RangeSubscriber, pa_self.range);
            pa_with (Run, intent, pa_self.limitedJoySpeed, pa_self.range, pa_self.speed);
            pa_with_weak (SpeedFilter, pa_self.speed, pa_self.filteredSpeed);
            pa_with_weak (Actuator, pa_self.filteredSpeed);
            pa_with_weak (Lights, pa_self.filteredSpeed);
            pa_with_weak (Logger, pa_self.limitedJoySpeed, pa_self.range, pa_self.filteredSpeed);
        } pa_co_end;

        pa_self.speed = {};
        stopActuator();
        stopLights();
        setLED(CRGB::Red);        
    }
} pa_end;

// Main

pa_activity (Main, pa_ctx(pa_co_res(4); Intent intent; pa_use(IntentPublisher);
                          pa_use(IntentRecognizer); pa_use(BlinkLED); pa_use(Receiver);                          
                          pa_use(Controller); pa_use(AccessPoint))) {
    Serial.println("Start");
    pa_co(2) {
        pa_with (AccessPoint);
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

    initLED();
    initLights();
}

void loop() {
    TickType_t prevWakeTime = xTaskGetTickCount();

    while (true) {
        M5.update();

        pa_tick(Main);

        showIfNeeded();

        // We run at 10 Hz.
        vTaskDelayUntil(&prevWakeTime, 100);
    }
}
