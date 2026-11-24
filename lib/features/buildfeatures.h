// Build features: which parts of BCP-clock a build contains, checks of their dependencies
// (c) 2025 Honza Skýpala
// WTFPL license applies
//
// The features are chosen by a build configuration (configs/*.ini) as -D BCP_FEATURE_<NAME>=0|1.
// Every feature must be defined, so a missing or misspelled one fails the build. Use #if, never #ifdef.

#pragma once

#if !defined(BCP_FEATURE_WIFIMGR) || !defined(BCP_FEATURE_MDNS) || !defined(BCP_FEATURE_BATTERY) || \
    !defined(BCP_FEATURE_CONNECTION_MONITOR)
#error "Build feature not defined, build with an env from configs/*.ini (see docs/features.md)"
#endif

// ---- Dependencies ----

#if !BCP_FEATURE_WIFIMGR
#error "The BCP countdown needs a network: enable BCP_FEATURE_WIFIMGR"
#endif

#if BCP_FEATURE_MDNS && !BCP_FEATURE_WIFIMGR
#error "BCP_FEATURE_MDNS needs a network: enable BCP_FEATURE_WIFIMGR"
#endif
