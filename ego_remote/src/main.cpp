// ego_remote
//
// Copyright (c) 2022, Framework Labs.

#include <ego_common.h>

#include <pa_stickc.h>
#include <pa_plankton.h>
#include <pa_utils.h>
#include <proto_activities.h>
#include <plankton.h>

#include <M5StickC.h>

// Screen

static TFT_eSprite screen{&M5.Lcd};
static auto needsDisplay = false;

static void initDisplay(uint8_t brightness = 10) {
    M5.Lcd.setRotation(0);
    M5.Axp.ScreenBreath(brightness);
    screen.createSprite(80, 160);
}

static void setNeedsDisplay() {
    needsDisplay = true;
}

static void displayIfNeeded() {
    if (needsDisplay) {
        screen.pushSprite(0, 0);
        needsDisplay = false;
    }
}

static void drawBorder(uint32_t color, unsigned width = 3) {
    for (unsigned i = 0; i < width; ++i) {
        const auto i2 = i * 2;
        screen.drawRect(i, i, 80 - i2, 160 - i2, color);
    }
    setNeedsDisplay();
}

static constexpr auto DIMMER_CONFIG = DimmerConfig{10, 50, 5};

// Input/Output Helpers

pa_activity (IntentSubscriber, pa_ctx(), Intent& intent) {
    plankton.subscribe(Topic::INTENT, {});
    pa_always {
        plankton.read(Topic::INTENT, (uint8_t*)&intent, 1);
    } pa_always_end;
} pa_end;

pa_activity (IntentChangeDetector, pa_ctx(Intent prevIntent), Intent intent, bool& intentChanged) {
    pa_self.prevIntent = intent;
    pa_always {
        if (intent != pa_self.prevIntent) {
            intentChanged = true;
            pa_self.prevIntent = intent;
        } else {
            intentChanged = false;
        }
    } pa_always_end;
} pa_end;

pa_activity (PressPublisher, pa_ctx(Press prevPress), Press press) {
    while (true) { 
        Serial.printf("pub press: %u\n", (uint8_t)press);
        plankton.publish(Topic::PRESS, (const uint8_t*)&press, 1);
        pa_self.prevPress = press;
        pa_await (press != pa_self.prevPress);
    }
} pa_end;

pa_activity (InputCombiner, pa_ctx(), bool stopButton, Intent intent, Press& press) {
    pa_always {
        if (intent != Intent::STOP && stopButton) {
            press = Press::SHORT;
        }
    } pa_always_end;
} pa_end;

// Joystick

pa_activity (JoystickReader, pa_ctx(), int8_t& x, int8_t& y, bool& btn) {
    pa_always {
        Wire.beginTransmission(0x38);
        Wire.write(0x02); // Register 2 
        Wire.endTransmission();
        Wire.requestFrom(0x38, 3);
        if (Wire.available()) {
            x = Wire.read();
            y = Wire.read();
            btn = Wire.read() == 0;
        }        
    } pa_always_end;
} pa_end;

pa_activity (JoystickLogger, pa_ctx(), int8_t x, int8_t y, bool btn) {
    pa_always {
        Serial.printf("x: %d\n", x);
        Serial.printf("y: %d\n", y);
        Serial.printf("btn: %u\n", btn);
    } pa_always_end;
} pa_end;

pa_activity (JoystickPublisher, pa_ctx(int8_t prevX; int8_t prevY; uint8_t prevBtn), int8_t x, int8_t y, bool btn) {
    while (true) { 
        {
            uint8_t buf[3];
            buf[0] = x;
            buf[1] = y;
            buf[2] = btn;
            plankton.publish(Topic::JOYSTICK, buf, 3);
        }
        pa_self.prevX = x;
        pa_self.prevY = y;
        pa_self.prevBtn = btn;
        pa_await (x != pa_self.prevX || y != pa_self.prevY || btn != pa_self.prevBtn);
    }
} pa_end;

// Screens

pa_activity (ErrorScreen, pa_ctx()) {
    Serial.println("Setup failed!");

    screen.fillSprite(RED);
    screen.setTextColor(WHITE);
    screen.setCursor(20, 75, 2);
    screen.print("ERROR");

    setNeedsDisplay();
    pa_halt;
} pa_end;

pa_activity (ConnectorScreen, pa_ctx(pa_use(Delay))) {
    screen.fillSprite(WHITE);
    screen.setTextColor(BLACK);
    screen.setCursor(10, 75, 1);
    screen.print("CONNECTING");

    drawBorder(ORANGE);

    while (true) {
        screen.fillCircle(40, 100, 10, ORANGE);
        setNeedsDisplay();
        pa_run (Delay, 5);

        screen.fillCircle(40, 100, 10, WHITE);
        setNeedsDisplay();
        pa_run (Delay, 5);
    }
} pa_end;

pa_activity (StopScreen, pa_ctx()) {
    screen.fillSprite(WHITE);
    screen.setTextColor(BLACK);

    screen.setCursor(5, 60, 1);
    screen.print("1x: Manual");

    screen.setCursor(5, 90, 1);
    screen.print("2x: Auto");

    drawBorder(RED);

    pa_halt;
} pa_end;

pa_activity (ManualScreen, pa_ctx(pa_co_res(2); pa_use(JoystickPublisher); pa_use(JoystickReader); pa_use(JoystickLogger)),
                           int8_t joyX, int8_t joyY) {
    screen.fillSprite(WHITE);
    screen.setTextColor(BLACK);

    screen.setCursor(15, 75, 2);
    screen.print("MANUAL");

    drawBorder(GREEN);

    pa_co(2) {
        pa_with (JoystickPublisher, joyX, joyY, false);
        pa_with (JoystickLogger, joyX, joyY, false);
    } pa_co_end;    
} pa_end;

pa_activity (AutoScreen, pa_ctx(pa_use(JoystickReader))) {
    screen.fillSprite(WHITE);
    screen.setTextColor(BLACK);

    screen.setCursor(20, 75, 2);
    screen.print("AUTO");

    drawBorder(BLUE);

    pa_halt;
} pa_end;

pa_activity (QuitScreen, pa_ctx(pa_use(Delay))) {
    screen.fillSprite(WHITE);
    screen.setTextColor(BLACK);
    screen.setCursor(20, 75, 2);
    screen.print("QUIT");

    screen.setCursor(5, 100, 1);
    screen.print("reset me!");

    while (true) {
        drawBorder(RED);
        pa_run (Delay, 5);
        
        drawBorder(WHITE);
        pa_run (Delay, 5);
    }
} pa_end;

pa_activity (MainScreen, pa_ctx(pa_use(StopScreen); pa_use(QuitScreen); pa_use(ManualScreen); pa_use(AutoScreen)), Intent intent, int8_t joyX, int8_t joyY) {
    while (true) {
        if (intent == Intent::STOP) {
            pa_when_abort(intent != Intent::STOP, StopScreen);
        }
        if (intent == Intent::START_MANU) {
            pa_when_abort (intent != Intent::START_MANU, ManualScreen, joyX, joyY);
        } 
        if (intent == Intent::START_AUTO) {
            pa_when_abort (intent != Intent::START_AUTO, AutoScreen);
        } 
        if (intent == Intent::QUIT) {
            pa_when_abort (intent != Intent::QUIT, QuitScreen);
        }
    }
} pa_end;

// Main Activity

pa_activity (Main, pa_ctx(pa_co_res(10); Press rawPress; Press press; Intent intent; bool intentChanged;
                          int8_t joyX; int8_t joyY; bool rawStopButton; bool stopButton;
                          pa_use(ErrorScreen); pa_use(PressRecognizer2); pa_use(JoystickReader);
                          pa_use(ConnectorScreen); pa_use(MainScreen); pa_use(InputCombiner);
                          pa_use(Connector); pa_use(Dimmer); pa_use(PressPublisher); pa_use(IntentChangeDetector);
                          pa_use(Receiver); pa_use(IntentSubscriber); pa_use(RaisingEdgeDetector)),
                   bool setupOK) {
    if (!setupOK) {
        pa_run (ErrorScreen);
    }

    pa_co(2) {
        pa_with (Connector);
        pa_with_weak (ConnectorScreen);
    } pa_co_end;

    pa_co(10) {
        pa_with (Receiver);
        pa_with (IntentSubscriber, pa_self.intent);
        pa_with (JoystickReader, pa_self.joyX, pa_self.joyY, pa_self.rawStopButton);
        pa_with (MainScreen, pa_self.intent, pa_self.joyX, pa_self.joyY);
        pa_with (PressRecognizer2, M5.BtnA, M5.BtnB, pa_self.rawPress);
        pa_with (IntentChangeDetector, pa_self.intent, pa_self.intentChanged);
        pa_with (Dimmer, DIMMER_CONFIG, pa_self.rawPress, pa_self.intentChanged, pa_self.press);
        pa_with (RaisingEdgeDetector, pa_self.rawStopButton, pa_self.stopButton);
        pa_with (InputCombiner, pa_self.stopButton, pa_self.intent, pa_self.press);
        pa_with (PressPublisher, pa_self.press);
    } pa_co_end;
} pa_end;

// Setup and Loop

static pa_use(Main);
static bool setupOK;

void setup() {
    setCpuFrequencyMhz(80);

    M5.begin();

    initDisplay();
    
    if (!Wire.begin(0, 26)) {
        Serial.println("Init Wire failed");
        return;
    }

    setupOK = true;
}

void loop() {
    TickType_t prevWakeTime = xTaskGetTickCount();

    while (true) {
        M5.update();

        pa_tick(Main, setupOK);

        displayIfNeeded();

        // We run at 10 Hz.
        vTaskDelayUntil(&prevWakeTime, 100);
    }
}
