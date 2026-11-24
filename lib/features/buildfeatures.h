// Build features: which parts of BCP-clock a build contains, checks of their dependencies
// (c) 2025 Honza Skýpala
// WTFPL license applies
//
// The features are chosen by a build configuration (configs/*.ini) as -D BCP_FEATURE_<NAME>=0|1, the modes
// as -D BCP_MODE_<NAME>=0|1.
// Every feature must be defined, so a missing or misspelled one fails the build. Use #if, never #ifdef.

#pragma once

#if !defined(BCP_FEATURE_WIFIMGR) || !defined(BCP_FEATURE_MDNS) || !defined(BCP_FEATURE_BATTERY) || \
    !defined(BCP_FEATURE_CONNECTION_MONITOR) || !defined(BCP_MODE_BCP) || !defined(BCP_MODE_OFFLINE)
#error "Build feature not defined, build with an env from configs/*.ini (see docs/features.md)"
#endif

// ---- Dependencies ----

#if !BCP_MODE_BCP && !BCP_MODE_OFFLINE
#error "A build needs at least one mode: enable BCP_MODE_BCP or BCP_MODE_OFFLINE"
#endif

#if BCP_MODE_BCP && !BCP_FEATURE_WIFIMGR
#error "BCP_MODE_BCP needs a network: enable BCP_FEATURE_WIFIMGR"
#endif

#if BCP_FEATURE_CONNECTION_MONITOR && !BCP_MODE_BCP
#error "BCP_FEATURE_CONNECTION_MONITOR monitors the BCP connection: enable BCP_MODE_BCP"
#endif

#if BCP_FEATURE_MDNS && !BCP_FEATURE_WIFIMGR
#error "BCP_FEATURE_MDNS needs a network: enable BCP_FEATURE_WIFIMGR"
#endif
