// Copyright (c) 2022, Framework Labs.

#include "pa_stickc.h"

#include <M5StickC.h>

// Dimmer

pa_activity_def (DimDownController, const DimmerConfig& config) {
    for (pa_self.brightness = config.maxBrightness; pa_self.brightness >= 7; --pa_self.brightness) {
        M5.Axp.ScreenBreath(pa_self.brightness);
        pa_run (Delay, config.stepPeriod);
    }
    pa_halt;
} pa_end;

pa_activity_def (DimmController, const DimmerConfig& config, bool wakeup, bool& isDimmed) {
    while (true) {
        pa_when_reset (wakeup, Delay, config.awakePeriod);
        isDimmed = true;
        pa_when_abort (wakeup, DimDownController, config);
        M5.Axp.ScreenBreath(config.maxBrightness);
        pa_pause;
        isDimmed = false;
    }
} pa_end;

pa_activity_def (PressCopyMachine, Press rawPress, bool isDimmed, Press& press) {
    pa_always {
        press = isDimmed ? Press::NO : rawPress;
    } pa_always_end;
} pa_end;

pa_activity_def (Dimmer, const DimmerConfig& config, Press rawPress, bool otherWakeup, Press& press) {
    pa_co(2) {
        pa_with (DimmController, config, (rawPress != Press::NO) || otherWakeup, pa_self.isDimmed);
        pa_with (PressCopyMachine, rawPress, pa_self.isDimmed, press);
    } pa_co_end;
} pa_end;
