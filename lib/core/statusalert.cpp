// Status alerts: icon cycle and connection problems, independent of the hardware
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "statusalert.h"

// ---- Alert icon cycle ----

int alertCyclePhase(int iconCount, unsigned long ms, DisplayState state) {
    if (iconCount <= 0 || isMessageShown(state)) return 0;
    return (ms / ALERT_PHASE_MS) % (iconCount + 1);
}

// ---- Connection problem ----

ConnectionAlert nextConnectionAlert(ConnectionAlert current, RefreshOutcome outcome) {
    ConnectionAlert next = current;
    switch (outcome) {
        case REFRESH_SUCCESS:
        case REFRESH_NO_REQUEST:
            return ConnectionAlert();
        case REFRESH_WIFI_DOWN:
            next.problem = CONNECTION_WIFI;
            break;
        case REFRESH_CONNECTION_ERROR:
        case REFRESH_HTTP_ERROR:
            next.problem = CONNECTION_BCP;
            break;
        case REFRESH_REBOOT:   // the device reboots right after, keep the problem
            break;
    }
    next.failures++;
    return next;
}
