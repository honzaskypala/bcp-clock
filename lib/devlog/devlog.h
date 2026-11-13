// Debug log for development builds (BCP_DEV) only
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

// DEV_LOG("Module", msg) prints "[Module] msg" to Serial in the dev build. In the production build
// msg is not evaluated and no code or strings are emitted; sizeof keeps variables used only in
// log messages from triggering "unused variable" warnings.
#ifdef BCP_DEV
#include <Arduino.h>
#define DEV_LOG(module, msg) do { Serial.print("[" module "] "); Serial.println(msg); } while (0)
#else
#define DEV_LOG(module, msg) do { (void) sizeof(msg); } while (0)
#endif
