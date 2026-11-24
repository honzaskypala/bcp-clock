// Serial command console for development builds (BCP_DEV) only
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#ifdef BCP_DEV

#include <buildfeatures.h>
#include <hw.h>

// Reads commands from Serial and executes them; call from loop()
void tickDevConsole(Hw *hw);

extern bool devWifiFailing;              // simulated WiFi outage, read by the BCP mode
const char *currentModeName();           // defined in main

#endif
