// Copyright (c) 2022, Framework Labs.

#include <pa_utils.h> // for Delay
#include <proto_activities.h>

// Dimmer

pa_activity_ctx (DimDownController, pa_use(Delay); uint8_t brightness);

pa_activity_ctx (DimmController, pa_use(Delay); pa_use(DimDownController));

pa_activity_ctx (PressCopyMachine);

pa_activity_ctx (Dimmer, pa_co_res(2); pa_use(PressCopyMachine); pa_use(DimmController); bool isDimmed);
