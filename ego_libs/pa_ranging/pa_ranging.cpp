// Copyright (c) 2022, Framework Labs.

#include "pa_ranging.h"

#include <VL53L0X.h>

// State

static VL53L0X sensor;

// Functions 

bool initRanging(int sda, int scl) {
    if (!Wire.begin(sda, scl, 100000)) {
        Serial.println("Wire init failed");
        return false;
    }
    if (!sensor.init()) {
        Serial.println("Sensor init failed");
        return false;
    }
    return true;
}

static void startRanging(void)
{
    sensor.startContinuous(100);
    sensor.setTimeout(50);
}

static uint16_t measureRange(void)
{
    const auto range = sensor.readRangeContinuousMillimeters();
    if (sensor.timeoutOccurred())
    {
        return UNDEF_RANGE;
    }
    return range;
}

void stopRanging(void)
{
    sensor.stopContinuous();
}

// Activities

pa_activity_def (Ranger, uint16_t& range) {
    startRanging();
    pa_always {
        range = measureRange();
    } pa_always_end;
} pa_end;
