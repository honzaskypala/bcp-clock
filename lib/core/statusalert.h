// Status alerts: icon cycle and connection problems, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <displaystate.h>
#include <policy.h>

// ---- Alert icon cycle ----

constexpr unsigned long ALERT_PHASE_MS = 5000;  // the content and each active icon are shown this long in turn

// Which part of the cycle is shown: 0 = content, 1..iconCount = that icon. Always the content when no icon is
// active or while the config page address scrolls after the mid button (it must not be interrupted).
int alertCyclePhase(int iconCount, unsigned long ms, DisplayState state);

// ---- Connection problem ----

constexpr int CONNECTION_ICON_AFTER_FAILURES = 4;  // with 5 failures to reboot: the last minute before it

enum ConnectionProblem {
    CONNECTION_OK,
    CONNECTION_WIFI,  // WiFi disconnected
    CONNECTION_BCP    // WiFi works, BCP connection or HTTP error
};

struct ConnectionAlert {
    ConnectionProblem problem = CONNECTION_OK;
    int failures = 0;  // consecutive failed refreshes of any kind; the last one decides the problem

    bool isIconDue() const { return problem != CONNECTION_OK && failures >= CONNECTION_ICON_AFTER_FAILURES; }
};

// Connection alert after a data refresh
ConnectionAlert nextConnectionAlert(ConnectionAlert current, RefreshOutcome outcome);
