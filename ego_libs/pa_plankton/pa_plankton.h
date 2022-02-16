// pa_plankton
//
// Copyright (c) 2022, Framework Labs.

#pragma once

#include <proto_activities.h>
#include <plankton.h>

extern Plankton plankton;

pa_activity_decl (Connector, pa_ctx());

pa_activity_decl (Receiver, pa_ctx());
