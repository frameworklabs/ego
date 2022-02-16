// pa_ranging
//
// Copyright (c) 2022, Framework Labs.

#pragma once

#include <proto_activities.h>

#include <cstdint>

// Constants

constexpr uint16_t UNDEF_RANGE = ((1 << 16) - 1);

// Functions

bool initRanging(int sda, int scl);

void stopRanging();

// Activities

pa_activity_decl (Ranger, pa_ctx(), uint16_t& range);
