// Serial command console for development builds (BCP_DEV) only
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#ifdef BCP_DEV

#include <hw.h>

// Reads commands from Serial and executes them; call from loop()
void devConsoleTick(Hw *hw);

#endif
