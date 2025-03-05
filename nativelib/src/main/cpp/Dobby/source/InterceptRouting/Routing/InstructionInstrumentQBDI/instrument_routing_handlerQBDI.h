#pragma once

#include "dobby/dobby_internal.h"

extern "C" {
void instrument_routing_dispatchQBDI(InterceptEntry *entry, DobbyRegisterContext *ctx);
}