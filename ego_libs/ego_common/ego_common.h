// ego_common
//
// Copyright (c) 2022, Framework Labs.

#pragma once

#include <cstdint>

// Common Types

enum class Intent : uint8_t {
    STOP = 0,
    START_MANU,
    START_AUTO,
    QUIT,
};

enum Topic : uint32_t {
    RANGE = 52,
    JOYSTICK = 53,
    INTENT = 54,
    PRESS = 55,
};
