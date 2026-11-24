// BCP mode: round countdown of a tournament on Best Coast Pairings
// (c) 2025 Honza Skýpala
// WTFPL license applies

#pragma once

#include <buildfeatures.h>

#if BCP_MODE_BCP

#include <Print.h>
#include <policy.h>
#if BCP_FEATURE_CONNECTION_MONITOR
#include <statusalert.h>
#endif
#include "mode.h"

class BcpMode : public Mode {
public:
    ModeId id() const override { return MODE_BCP; }
    const char *name() const override { return "BCP"; }
    bool needsNetwork() const override { return true; }
    void enter(Hw& hw) override;
    void leave(Hw& hw) override;
    void tick(Hw& hw) override;
    ModeAction onButton(Hw& hw, ButtonEvent event) override;
    void onConfigUpdated(Hw& hw) override;

#if defined(BCP_DEV) && BCP_FEATURE_CONNECTION_MONITOR
    void printConnectionStatus(Print& out);
#endif

private:
    bool idSet_ = false;                 // BCPEvent.setID() called since boot
    unsigned long lastDataRefresh_ = 0;  // millis() of the last refresh, 0 = refresh now
    DisplayMemory memory_;
#if BCP_FEATURE_CONNECTION_MONITOR
    int failCount_ = 0;                  // consecutive WiFi outages and connection failures
    ConnectionAlert connectionAlert_;    // status indicator and icon of a connection problem
#endif

    bool isRefreshDue(Hw& hw);
    void refreshEventData(Hw& hw);
    void updateDisplay(Hw& hw);
    void applyDisplayAction(Hw& hw, DisplayAction action);
    void resetConnectionAlert(Hw& hw);
    CountdownDisplay countdown();
};

extern BcpMode bcpMode;

#endif
