// pa_stickc
//
// Copyright (c) 2022, Framework Labs.

#pragma once

#include "pa_stickc_priv.h"

#include <pa_utils.h> // for Press

// Dimmer

struct DimmerConfig {
    uint8_t maxBrightness;
    unsigned awakePeriod;
    unsigned stepPeriod;
};

pa_activity_sig (Dimmer, const DimmerConfig& config, Press rawPress, bool otherWakeup, Press& press);
