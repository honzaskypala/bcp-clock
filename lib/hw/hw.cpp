// Abstract interface for hardware specific implementation
// You need to implement this interface for your hardware platform.
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "hw.h"

volatile DisplayState Hw::displayState = DISPLAY_BOOT;
volatile bool Hw::enforceUpdate = false;
volatile bool Hw::redrawRequested = false;