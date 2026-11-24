// Serial command console for development builds (BCP_DEV) only
// (c) 2025 Honza Skýpala
// WTFPL license applies

#ifdef BCP_DEV

#include "devconsole.h"

#include <Arduino.h>
#include <WiFi.h>
#include <sys/time.h>
#include <config.h>
#include <tc001.h>
#if BCP_MODE_BCP
#include <bcpevent.h>
#include "bcpmode.h"
#endif

static long timeShift_ = 0;  // total seconds the system clock was shifted by "timeshift"
bool devWifiFailing = false;

// ---- Commands ----

static void handleHelp() {
    Serial.println("Commands:");
    Serial.println("  help              this list");
    Serial.println("  status            device, network, mode, event and timer state");
    Serial.println("  screen            ASCII dump of the LED matrix");
    Serial.println("  left | right      press the left / right button");
    Serial.println("  mid | midlong     short (next mode) / long (config page) press of the middle button");
    Serial.println("  timeshift <sec>   shift system clock by <sec> seconds (negative to go back)");
#if BCP_MODE_BCP
    Serial.println("  refresh           refresh BCP data now");
    Serial.println("  fake <mode> [sec] [length] [round/rounds]");
    Serial.println("                    fake event data, BCP is not contacted until 'fake off'");
    Serial.println("                    modes: name (not started), round (no timer), start (sec to round start),");
    Serial.println("                    run (sec remaining), pause (sec remaining), over (sec over time), off");
    Serial.println("                    defaults: sec=300, length=9000, round 3/5");
    Serial.println("  bcpfail on|off    simulate BCP connection errors (counts toward reboot, reset by reboot)");
    Serial.println("  wififail on|off   simulate a WiFi outage (counts toward reboot, reset by reboot)");
#endif
    Serial.println("  beep yellow|red   play the threshold signal (also when acoustic signals are off)");
#if BCP_FEATURE_BATTERY
    Serial.println("  battery [raw|off] battery reading; raw simulates the ADC median (e.g. 2300), off = real");
    Serial.println("  battlog [start [sec]|stop]  battery calibration log in NVS (default every 60 s), no arg = CSV");
#endif
    Serial.println("  portal            reboot into the captive portal (= Mid held at boot)");
    Serial.println("  reboot            restart the device");
}

static void handleStatus(Hw *hw) {
    time_t now = time(nullptr);
    Serial.printf("uptime: %lu s, free heap: %u B, min free heap: %u B\n",
                  millis() / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    Serial.printf("time: %ld (UTC %s", (long) now, asctime(gmtime(&now)));  // asctime adds '\n'
    Serial.printf("timeshift: %ld s\n", timeShift_);
    Serial.printf("wifi: %s, ssid: %s, ip: %s, rssi: %d dBm\n",
                  WiFi.isConnected() ? "connected" : "disconnected",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    Serial.printf("mode: %s, display state: %s, countdown draws: %u\n", currentModeName(),
                  displayStateName(hw->displayState), Tc001::countdownDraws);
    Serial.printf("config server: %s\n", Config.isConfigServerRunning() ? "running" : "stopped");
    Serial.printf("config: event=%s yellow=%ld red=%ld beep=%s\n",
                  Config.eventId().c_str(), Config.yellowThreshold(), Config.redThreshold(),
                  Config.isAcousticSignalsEnabled() ? "on" : "off");
#if BCP_MODE_BCP
    Serial.printf("event: id=%s valid=%d invalid=%d fake=%d bcpfail=%d lastStatus=%d name=\"%s\"\n",
                  BCPEvent.id().c_str(), BCPEvent.isValid(), BCPEvent.isInvalid(), BCPEvent.isFake(),
                  BCPEvent.isNetworkFailing(), BCPEvent.lastStatus(), BCPEvent.name().c_str());
#if BCP_FEATURE_CONNECTION_MONITOR
    bcpMode.printConnectionStatus(Serial);
#endif
    Serial.printf("event: started=%d ended=%d round=%d/%d\n",
                  BCPEvent.isStarted(), BCPEvent.isEnded(), BCPEvent.currentRound(), BCPEvent.numberOfRounds());
    Serial.printf("timer: length=%u start=%s end=%s paused=%d pausedRemaining=%u\n",
                  BCPEvent.timerLength(), BCPEvent.roundStartTime().c_str(), BCPEvent.roundEndTime().c_str(),
                  BCPEvent.isTimerPaused(), BCPEvent.pausedTimeRemaining());
    if (BCPEvent.timerLength() > 0) {
        Serial.printf("timer: remaining=%ld s\n", (long) (BCPEvent.roundEndEpoch() - now));
    }
#endif
}

static void handleTimeshift(const String& arg) {
    long shift = arg.toInt();
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    tv.tv_sec += shift;
    settimeofday(&tv, nullptr);
    timeShift_ += shift;
    Serial.printf("Clock shifted by %ld s (total %ld s)\n", shift, timeShift_);
}

#if BCP_MODE_BCP
// Returns the n-th space separated word of str, or empty string
static String extractWord(const String& str, int n) {
    int start = 0;
    for (int i = 0; i < n; i++) {
        start = str.indexOf(' ', start);
        if (start < 0) return String();
        while (str[start] == ' ') start++;
    }
    int end = str.indexOf(' ', start);
    return end < 0 ? str.substring(start) : str.substring(start, end);
}

static void handleFake(const String& arg) {
    String mode = extractWord(arg, 0);
    if (mode == "off") {
        BCPEvent.clearFakeData();
        Hw::enforceUpdate = true;
        Serial.println("Fake data off, refreshing from BCP");
        return;
    }

    String secArg = extractWord(arg, 1), lengthArg = extractWord(arg, 2), roundArg = extractWord(arg, 3);
    long sec = secArg.length() > 0 ? secArg.toInt() : 300;
    long length = lengthArg.length() > 0 ? lengthArg.toInt() : 9000;
    int round = 3, rounds = 5;
    int slash = roundArg.indexOf('/');
    if (slash > 0) {
        round = roundArg.substring(0, slash).toInt();
        rounds = roundArg.substring(slash + 1).toInt();
    }
    time_t now = time(nullptr);

    if (mode == "name") BCPEvent.setFakeData(false, 0, rounds, 0, 0, false, 0);
    else if (mode == "round") BCPEvent.setFakeData(true, round, rounds, 0, 0, false, 0);
    else if (mode == "start") BCPEvent.setFakeData(true, round, rounds, length, now + length + sec, false, 0);
    else if (mode == "run") BCPEvent.setFakeData(true, round, rounds, length, now + sec, false, 0);
    else if (mode == "pause") BCPEvent.setFakeData(true, round, rounds, length, now + sec, true, sec);
    else if (mode == "over") BCPEvent.setFakeData(true, round, rounds, length, now - sec, false, 0);
    else {
        Serial.println("Unknown fake mode, type help");
        return;
    }
    Hw::enforceUpdate = true;
    Serial.printf("Fake data set: %s sec=%ld length=%ld round=%d/%d\n", mode.c_str(), sec, length, round, rounds);
}
#endif

static void executeCommand(const String& line, Hw *hw) {
    int space = line.indexOf(' ');
    String cmd = space < 0 ? line : line.substring(0, space);
    String arg = space < 0 ? String() : line.substring(space + 1);
    arg.trim();

    if (cmd == "help") handleHelp();
    else if (cmd == "status") handleStatus(hw);
    else if (cmd == "screen") Tc001::dumpScreen(Serial);
    else if (cmd == "left") { Tc001::simulateButton(BUTTON_LEFT); Serial.println("Left button pressed"); }
    else if (cmd == "right") { Tc001::simulateButton(BUTTON_RIGHT); Serial.println("Right button pressed"); }
    else if (cmd == "mid") { Tc001::simulateButton(BUTTON_MID_SHORT); Serial.println("Mid button pressed (short)"); }
    else if (cmd == "midlong") { Tc001::simulateButton(BUTTON_MID_LONG); Serial.println("Mid button pressed (long)"); }
    else if (cmd == "timeshift") handleTimeshift(arg);
#if BCP_MODE_BCP
    else if (cmd == "refresh") { Hw::enforceUpdate = true; Serial.println("Refresh requested"); }
    else if (cmd == "fake") handleFake(arg);
    else if (cmd == "bcpfail") {
        BCPEvent.setNetworkFailing(arg == "on");
        Serial.printf("BCP connection errors %s\n", BCPEvent.isNetworkFailing() ? "on" : "off");
    }
    else if (cmd == "wififail") {
        devWifiFailing = arg == "on";
        BCPEvent.setNetworkFailing(devWifiFailing);
        Serial.printf("WiFi outage %s\n", devWifiFailing ? "on" : "off");
    }
#endif
    else if (cmd == "beep" && (arg == "yellow" || arg == "red")) {
        Tc001::playBeep(arg == "red" ? BEEP_RED : BEEP_YELLOW);
        Serial.printf("Playing %s signal\n", arg.c_str());
    }
#if BCP_FEATURE_BATTERY
    else if (cmd == "battery") {
        if (arg == "off") Tc001::simulateBattery(-1);
        else if (arg.length() > 0) Tc001::simulateBattery(constrain(arg.toInt(), 0, 4095));
        Tc001::printBattery(Serial);
    }
    else if (cmd == "battlog") {
        if (arg.startsWith("start")) {
            int sec = arg.length() > 5 ? arg.substring(5).toInt() : 60;
            Tc001::startBatteryLog(constrain(sec, 1, 3600));
            Serial.printf("Battery log started, every %d s (previous log erased)\n", constrain(sec, 1, 3600));
        }
        else if (arg == "stop") { Tc001::stopBatteryLog(); Serial.println("Battery log stopped"); }
        else Tc001::printBatteryLog(Serial);
    }
#endif
    else if (cmd == "portal") { Serial.println("Rebooting into portal..."); Serial.flush(); static_cast<Tc001 *>(hw)->rebootToPortal(); }
    else if (cmd == "reboot") { Serial.println("Rebooting..."); Serial.flush(); hw->reboot(); }
    else Serial.println("Unknown command, type help");
}

// ---- Public entry point ----

void tickDevConsole(Hw *hw) {
    static String line;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            line.trim();
            if (line.length() > 0) {
                Serial.println("> " + line);
                executeCommand(line, hw);
                Serial.println("[Console] done");
            }
            line = "";
        } else if (line.length() < 128) {
            line += c;
        }
    }
}

#endif
