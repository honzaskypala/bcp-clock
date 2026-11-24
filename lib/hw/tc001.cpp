// Hardware interface implementation for Ulanzi TC001 desktop clock
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include "tc001.h"

#include <f3x5.h>
#include <f4x6.h>
#include <wifimgr.h>
#include <config.h>
#include <bcpevent.h>
#include <countdown.h>
#include <policy.h>
#ifdef BCP_DEV
#include <Preferences.h>
#endif

#define PIN_BUZZER          15
#define PIN_MID_BUTTON      27
#define PIN_RIGHT_BUTTON    14
#define PIN_LED_MATRIX      32
#define PIN_BATTERY         34  // battery voltage, ADC1

#define COLOR_WHITE matrix.Color24to16(CRGB::White)
#define COLOR_BLACK matrix.Color24to16(CRGB::Black)
#define COLOR_RED matrix.Color24to16(CRGB::Red)
#define COLOR_YELLOW matrix.Color24to16(CRGB::Yellow)
#define COLOR_GREEN matrix.Color24to16(CRGB::Green)
#define COLOR_MAGENTA matrix.Color24to16(CRGB::Magenta)
#define COLOR_AMBER matrix.Color(255, 140, 0)  // (255, 77, 0) on the LEDs after the gamma correction
#define COLOR_DEEPSKYBLUE matrix.Color24to16(CRGB::DeepSkyBlue)
#define COLOR_DARKGRAY matrix.Color(20, 20, 20)

#define SCROLLING_TEXT_SPEED 100 // miliseconds per step
#define PROGRESS_INDICATOR_SPEED 100 // milliseconds per step
#define INDICATOR_BLINK_MS 1000      // status indicators: on for this long, then off for this long
#define BATTERY_READ_INTERVAL_MS 1000

constexpr uint32_t BEEP_FREQ = 2000;   // Hz, the buzzer is passive and needs a square wave
constexpr uint8_t BEEP_CHANNEL = 0;    // LEDC channel (FastLED uses RMT, not LEDC)

constexpr int DEBOUNCE_DELAY_MS = 500;
constexpr uint32_t PORTAL_TIMEOUT_MS = 5UL * 60UL * 1000UL; // captive portal inactivity timeout, then reboot and retry

CRGB Tc001::matrixleds[256];
FastLED_NeoMatrix Tc001::matrix = FastLED_NeoMatrix(Tc001::matrixleds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);

const UTF8_32BitFont* Tc001::defaultFont = &F3x5[0];
const GFXfont* Tc001::hmmssFont = &F3x5_Fixed;
const GFXfont* Tc001::mmssFont = &F4x6;

unsigned long Tc001::stMillis = 0;
unsigned long Tc001::piMillis = 0;
time_t Tc001::countdownDrawnAt = 0;
long Tc001::lastRemaining = LONG_MIN;
BeepSignal Tc001::beepSignal = BEEP_NONE;
unsigned long Tc001::beepStartMs = 0;
bool Tc001::beepToneOn = false;
volatile bool Tc001::indicatorActive[INDICATOR_COUNT] = {};
volatile bool Tc001::iconActive[ICON_COUNT] = {};
uint8_t Tc001::shownOverlay = 0;
BatteryMonitor Tc001::battery;
volatile BatteryLevel Tc001::batteryLevel = BATTERY_UNKNOWN;
unsigned long Tc001::batteryReadAt = 0;

// ---- Display lock ----
// The matrix and the animations are used by both loop() and handleEvents (same core and priority,
// so they preempt each other). All display access goes through this recursive mutex.
static SemaphoreHandle_t displayMutex = nullptr;  // created in the Tc001 constructor

class DisplayLock {
public:
    DisplayLock() { xSemaphoreTakeRecursive(displayMutex, portMAX_DELAY); }
    ~DisplayLock() { xSemaphoreGiveRecursive(displayMutex); }
    DisplayLock(const DisplayLock&) = delete;
    DisplayLock& operator=(const DisplayLock&) = delete;
};

const CBCPEvent* Tc001::countdownEvent = nullptr;
ProgressIndicator* Tc001::progressIndicator = nullptr;
ScrollingText* Tc001::scrollingText = nullptr;
bool Tc001::midButtonPressed = false;

Tc001::Tc001() {
    pinMode(PIN_BUZZER, INPUT_PULLDOWN);      // stop whistle noise
    pinMode(PIN_MID_BUTTON, INPUT_PULLUP);    // mid button
    pinMode(PIN_RIGHT_BUTTON, INPUT_PULLUP);  // right button
    Serial.begin(9600);
    while (!Serial);

    FastLED.addLeds<NEOPIXEL,PIN_LED_MATRIX>(matrixleds, 256);
    matrix.begin();
    matrix.setTextWrap(false);
    // matrix.setBrightness(40);
    matrix.setTextColor(matrix.Color(255, 255, 255));
    matrix.setFont((GFXfont *) defaultFont);

    displayMutex = xSemaphoreCreateRecursiveMutex();
    xTaskCreatePinnedToCore(
            handleEvents,     // Function to run
            "EventHandler",   // Task name
            4096*2,           // Stack size
            NULL,             // Parameter
            1,                // Priority
            NULL,             // Task handle
            1                 // Core (0 or 1)
        );
}

void Tc001::stopAnimations() {
    if (scrollingText && scrollingText->isActive) {
        scrollingText->isActive = false;
        delete scrollingText;
        scrollingText = nullptr;
    }
    stopProgress();
    stMillis = 0;
    piMillis = 0;
    countdownDrawnAt = 0;
}

void Tc001::showSplashScreen(bool showProgress) {
    DisplayLock lock;
    displayState = DISPLAY_BOOT;
    stopAnimations();
    matrix.clear();
    int y = showProgress ? 6 : 7;
    matrix.setCursor(0, y);
    matrix.setTextColor(COLOR_WHITE);
    matrix.print("BCP");
    int16_t x1, y1;
    uint16_t w, h;
    matrix.getTextBounds("clock", 0, y, &x1, &y1, &w, &h);
    matrix.setCursor((matrix.width() - w), y);  
    matrix.print("clock");
    if (showProgress) {
        startProgress();
    }
    showMatrix();
}

void Tc001::showConfigServerMsg(const char *msg, bool loop) {
    DisplayLock lock;
    displayState = loop ? DISPLAY_CONFIG_REQUIRED : DISPLAY_CONFIG_MESSAGE;
    stopAnimations();
    matrix.clear();
    String msgStr = "Config page http://" + WiFi.localIP().toString();
    if (msg != nullptr && msg[0] != '\0') {
        // red message shown once, then the config page address keeps looping
        scrollingText = new ScrollingText(&matrix, msg, 0, 7, (const GFXfont *) defaultFont, false, COLOR_RED, 24, 0, loop);
        scrollingText->append(msgStr, COLOR_WHITE, true, 24, 8);
    } else {
        scrollingText = new ScrollingText(&matrix, msgStr.c_str(), 0, 7, (const GFXfont *) defaultFont, false, COLOR_WHITE, 24, 8, loop);
    }
    showMatrix();
}

void Tc001::showConfigServerMsg(const char *msg) {
    showConfigServerMsg(msg, true);
}

void Tc001::displayRoundWithCountdown(int currentRound, int totalRounds) {
    const int line = 7;
    const int displayWidth = 32;
    RoundPixel pixels[displayWidth];
    layoutRoundIndicator(currentRound, totalRounds, displayWidth, pixels);
    for (int x = 0; x < displayWidth; x++) {
        uint16_t color = pixels[x] == ROUND_PIXEL_ACTIVE ? COLOR_DEEPSKYBLUE
                       : pixels[x] == ROUND_PIXEL_INACTIVE ? COLOR_DARKGRAY : COLOR_BLACK;
        matrix.drawPixel(x, line, color);
    }
}

void Tc001::displayEventName(const CBCPEvent& event) {
    DisplayLock lock;
    displayState = DISPLAY_EVENT_NAME;
    stopAnimations();
    matrix.clear();
    scrollingText = new ScrollingText(&matrix, event.name(), 0, 7, defaultFont, true);
    showMatrix();
}

void Tc001::displayEventRound(const CBCPEvent& event) {
    DisplayLock lock;
    displayState = DISPLAY_EVENT_ROUND;
    stopAnimations();
    matrix.clear();
    matrix.setTextColor(COLOR_WHITE);
    int16_t  x1, y1;
    uint16_t w, h;
    char msg[20];
    snprintf(msg, sizeof(msg), "Round %d", event.currentRound());
    matrix.getTextBounds(msg, 0, 7, &x1, &y1, &w, &h);
    matrix.setCursor((matrix.width() - w) / 2, 7);
    matrix.print(msg);
    showMatrix();
}

void Tc001::displayCountdown(const CBCPEvent& event) {
    DisplayLock lock;
    stopAnimations();  // also resets countdownDrawnAt, so handleEvents draws the countdown right away
    countdownEvent = &event;
    lastRemaining = LONG_MIN;  // the first redraw does not beep (boot or return in the middle of a round)
    displayState = DISPLAY_EVENT_COUNTDOWN;
}

void Tc001::drawCountdown(const CBCPEvent& event) {
#ifdef BCP_DEV
    countdownDraws++;
#endif
    time_t now = time(nullptr);
    long remaining = event.roundEndEpoch() - now;
    CountdownView view = formatCountdown(remaining, event.timerLength(), event.isTimerPaused(),
                                         event.pausedTimeRemaining(), Config.yellowThreshold(),
                                         Config.redThreshold(), now % 2);

    // the same value formatCountdown compares with the thresholds
    if (event.isTimerPaused()) remaining = event.pausedTimeRemaining();
    BeepSignal signal = detectThresholdCrossing(lastRemaining, remaining, Config.yellowThreshold(),
                                                Config.redThreshold());
    lastRemaining = remaining;
    if (signal != BEEP_NONE && Config.isAcousticSignalsEnabled()) {
        startBeep(signal);
    }
    uint16_t color;
    switch (view.color) {
        case COUNTDOWN_YELLOW:  color = COLOR_YELLOW; break;
        case COUNTDOWN_RED:     color = COLOR_RED; break;
        case COUNTDOWN_GREEN:   color = COLOR_GREEN; break;
        case COUNTDOWN_MAGENTA: color = COLOR_MAGENTA; break;
        case COUNTDOWN_BLACK:   color = COLOR_BLACK; break;
        default:                color = COLOR_WHITE; break;
    }
    matrix.setTextColor(color);
    matrix.setFont(view.hmmss ? hmmssFont : mmssFont);

    matrix.clear();
    matrix.setCursor(view.x, 6);
    matrix.print(view.text);
    displayRoundWithCountdown(event.currentRound(), event.numberOfRounds());
    showMatrix();
    matrix.setFont((GFXfont *) defaultFont);
}

// ---- Buzzer ----
// Called from handleEvents only (the dev console requests a signal through playBeep()).

void Tc001::startBeep(BeepSignal signal) {
    if (beepSignal == BEEP_NONE) {
        ledcSetup(BEEP_CHANNEL, BEEP_FREQ, 8);
        ledcAttachPin(PIN_BUZZER, BEEP_CHANNEL);
    }
    beepToneOn = false;
    beepStartMs = millis();
    beepSignal = signal;
}

// Switches the tone on/off by the signal pattern; the pin is pulled down again when the pattern ends
void Tc001::stepBeep() {
#ifdef BCP_DEV
    if (beepRequest != BEEP_NONE) {
        startBeep(beepRequest);
        beepRequest = BEEP_NONE;
    }
#endif
    BeepSignal signal = beepSignal;
    if (signal == BEEP_NONE) return;
    unsigned long elapsed = millis() - beepStartMs;
    bool on = isBeepOn(signal, elapsed);
    if (on != beepToneOn) {
        ledcWriteTone(BEEP_CHANNEL, on ? BEEP_FREQ : 0);
        beepToneOn = on;
    }
    if (elapsed >= beepPatternLength(signal)) {
        ledcDetachPin(PIN_BUZZER);
        pinMode(PIN_BUZZER, INPUT_PULLDOWN);  // stop whistle noise
        beepSignal = BEEP_NONE;
    }
}

void Tc001::startProgress() {
    if (progressIndicator == nullptr) {
        progressIndicator = new ProgressIndicator(&matrix, 0, matrix.height() - 1, matrix.width(), COLOR_WHITE, COLOR_BLACK);
    }
    progressIndicator->isActive = true;
}

void Tc001::stepProgress() {
    if (progressIndicator != nullptr && progressIndicator->isActive) {
        progressIndicator->step(true);
    }
}

void Tc001::stopProgress() {
    if (progressIndicator != nullptr && progressIndicator->isActive) {
        progressIndicator->hide();
        progressIndicator->isActive = false;
    }
}

// ---- Status overlay ----
// Called with the display lock held.

static const uint8_t batteryIcon[] = {  // 15x7, empty battery with "LoW"
    0x7F, 0xFE,  // .##############
    0x40, 0x02,  // .#............#
    0xD1, 0x2A,  // ##.#...#..#.#.#
    0xD2, 0xBA,  // ##.#..#.#.###.#
    0xD9, 0x3A,  // ##.##..#..###.#
    0x40, 0x02,  // .#............#
    0x7F, 0xFE   // .##############
};
static const uint8_t wifiIcon[] = {  // 8x7
    0x3C,  // ..####..
    0x42,  // .#....#.
    0x18,  // ...##...
    0x24,  // ..#..#..
    0x00,  // ........
    0x18,  // ...##...
    0x18   // ...##...
};
static const uint8_t wifiOffText[] = {  // 25x8, drawn over wifiIcon: strike-through and "Wi-Fi"
    0x01, 0x00, 0x00, 0x00,  // .......#.................
    0x02, 0x00, 0x00, 0x00,  // ......#..................
    0x04, 0x55, 0x0E, 0x80,  // .....#...#.#.#.#....###.#
    0x08, 0x54, 0x08, 0x00,  // ....#....#.#.#......#....
    0x10, 0x55, 0x6C, 0x80,  // ...#.....#.#.#.#.##.##..#
    0x20, 0x55, 0x08, 0x80,  // ..#......#.#.#.#....#...#
    0x40, 0x29, 0x08, 0x80,  // .#........#.#..#....#...#
    0x80, 0x00, 0x00, 0x00   // #........................
};
static const uint8_t bcpIcon[] = {  // 24x5, unplugged plug and socket
    0x0F, 0x03, 0xC0,  // ....####......####......
    0x0F, 0xE1, 0xC0,  // ....#######....###......
    0xFF, 0x03, 0xFF,  // ########......##########
    0x0F, 0xE1, 0xC0,  // ....#######....###......
    0x0F, 0x03, 0xC0   // ....####......####......
};
static CRGB overlayBackup[256];  // content under the overlay while it is shown

uint8_t Tc001::overlayState() {
    uint8_t icons[ICON_COUNT];
    int iconCount = 0;
    for (int i = 0; i < ICON_COUNT; i++) {
        if (iconActive[i]) icons[iconCount++] = i;
    }
    int phase = alertCyclePhase(iconCount, millis(), displayState);
    if (phase > 0) return OVERLAY_ICON | icons[phase - 1];
    uint8_t mask = 0;
    for (int i = 0; i < INDICATOR_COUNT; i++) {
        if (indicatorActive[i]) mask |= 1 << i;
    }
    if ((millis() / INDICATOR_BLINK_MS) % 2 == 1) return 0;
    return mask;
}

bool Tc001::drawOverlay(uint8_t state) {
    if (state == 0) return false;
    memcpy(overlayBackup, matrixleds, sizeof(overlayBackup));
    if (state & OVERLAY_ICON) {
        matrix.fillScreen(COLOR_BLACK);
        switch (state & ~OVERLAY_ICON) {
            case ICON_BATTERY: matrix.drawBitmap(8, 1, batteryIcon, 15, 7, COLOR_RED); break;  // body in columns 9..22
            case ICON_WIFI:  // 25x8 in columns 3..27
                matrix.drawBitmap(3, 1, wifiIcon, 8, 7, COLOR_AMBER);
                matrix.drawBitmap(3, 0, wifiOffText, 25, 8, COLOR_RED);
                break;
            case ICON_BCP: matrix.drawBitmap(4, 2, bcpIcon, 24, 5, COLOR_MAGENTA); break;
        }
        return true;
    }
    static const uint16_t indicatorColors[INDICATOR_COUNT] = { COLOR_RED, COLOR_AMBER, COLOR_MAGENTA };
    int y = 0;
    for (int i = 0; i < INDICATOR_COUNT; i++) {
        if (state & (1 << i)) matrix.drawPixel(0, y++, indicatorColors[i]);
    }
    return true;
}

void Tc001::restoreOverlay() {
    memcpy(matrixleds, overlayBackup, sizeof(overlayBackup));
}

void Tc001::showMatrix() {
    shownOverlay = overlayState();
    bool drawn = drawOverlay(shownOverlay);
    matrix.show();
    if (drawn) restoreOverlay();
}

// ---- Battery ----
// Called from handleEvents only.

void Tc001::stepBattery() {
    unsigned long now = millis();
    if (batteryReadAt != 0 && now - batteryReadAt < BATTERY_READ_INTERVAL_MS) return;
    batteryReadAt = now;
    uint16_t raw = analogRead(PIN_BATTERY);
    battery.addSample(raw);
    BatteryLevel level = battery.level();
#ifdef BCP_DEV
    batteryRaw = raw;
    stepBatteryLog();
    if (simulatedBatteryRaw >= 0) level = nextBatteryLevel(BATTERY_UNKNOWN, simulatedBatteryRaw);
#endif
    batteryLevel = level;
    indicatorActive[INDICATOR_LOW_BATTERY] = level == BATTERY_LOW || level == BATTERY_CRITICAL;
    iconActive[ICON_BATTERY] = level == BATTERY_CRITICAL;
}

// ---- Connection alert ----

void Tc001::setConnectionAlert(ConnectionProblem problem, bool showIcon) {
    indicatorActive[INDICATOR_WIFI] = problem == CONNECTION_WIFI;
    indicatorActive[INDICATOR_BCP] = problem == CONNECTION_BCP;
    iconActive[ICON_WIFI] = showIcon && problem == CONNECTION_WIFI;
    iconActive[ICON_BCP] = showIcon && problem == CONNECTION_BCP;
}

void Tc001::reboot() {
    ESP.restart();
}

#ifdef BCP_DEV
uint32_t Tc001::countdownDraws = 0;
volatile BeepSignal Tc001::beepRequest = BEEP_NONE;
volatile int Tc001::simulatedBatteryRaw = -1;
uint16_t Tc001::batteryRaw = 0;

// ---- Battery calibration log ----
// Runs in handleEvents. The whole log is one NVS blob rewritten at every sample (2 kB, fine for a one-off
// discharge measurement); the recording state is restored after a restart.

static constexpr int BATTERY_LOG_SIZE = 1024;
static uint16_t batteryLog[BATTERY_LOG_SIZE];
static uint16_t batteryLogCount = 0;
static uint16_t batteryLogInterval = 60;    // seconds
static bool batteryLogRecording = false;
static unsigned long batteryLogAt = 0;      // millis() of the last sample
static bool batteryLogLoaded = false;
volatile int Tc001::batteryLogRequest = 0;

static void saveBatteryLog() {
    Preferences prefs;
    prefs.begin("battlog", false);
    prefs.putBool("on", batteryLogRecording);
    prefs.putUShort("interval", batteryLogInterval);
    prefs.putUShort("count", batteryLogCount);
    prefs.putBytes("data", batteryLog, batteryLogCount * sizeof(uint16_t));
    prefs.end();
}

void Tc001::stepBatteryLog() {
    if (!batteryLogLoaded) {
        batteryLogLoaded = true;
        Preferences prefs;
        prefs.begin("battlog", true);
        batteryLogRecording = prefs.getBool("on", false);
        batteryLogInterval = prefs.getUShort("interval", 60);
        batteryLogCount = prefs.getUShort("count", 0);
        if (batteryLogCount > BATTERY_LOG_SIZE) batteryLogCount = 0;
        prefs.getBytes("data", batteryLog, batteryLogCount * sizeof(uint16_t));
        prefs.end();
    }
    int request = batteryLogRequest;
    if (request != 0) {
        batteryLogRequest = 0;
        if (request > 0) {
            batteryLogCount = 0;
            batteryLogInterval = request;
            batteryLogRecording = true;
            batteryLogAt = millis();
        } else {
            batteryLogRecording = false;
        }
        saveBatteryLog();
    }
    if (!batteryLogRecording || batteryLogCount >= BATTERY_LOG_SIZE) return;
    if (millis() - batteryLogAt < batteryLogInterval * 1000UL) return;
    uint16_t median = battery.filteredRaw();
    if (median == 0) return;  // window not full yet after a restart
    batteryLogAt += batteryLogInterval * 1000UL;
    batteryLog[batteryLogCount++] = median;
    saveBatteryLog();
}

void Tc001::printBatteryLog(Print& out) {
    out.printf("# battery log: %u samples, interval %u s, %s\n", batteryLogCount, batteryLogInterval,
               batteryLogRecording ? "recording" : "stopped");
    out.println("seconds,raw");
    for (int i = 0; i < batteryLogCount; i++) {
        out.printf("%lu,%u\n", (unsigned long)(i + 1) * batteryLogInterval, batteryLog[i]);
    }
}

static constexpr uint32_t FORCE_PORTAL_MAGIC = 0xB0C1F0CE;
RTC_NOINIT_ATTR static uint32_t forcePortalFlag;  // survives ESP.restart()

void Tc001::rebootToPortal() {
    forcePortalFlag = FORCE_PORTAL_MAGIC;
    reboot();
}

// Prints the LED matrix as ASCII, one character per pixel:
// '.' off, ':' dim, W white, R red, A amber, G green, B blue, Y yellow, M magenta, C cyan
// Stored values are gamma corrected, so channels are compared relative to the strongest one.
// Includes the status overlay as it was last shown.
void Tc001::dumpScreen(Print& out) {
    DisplayLock lock;
    bool overlayDrawn = drawOverlay(shownOverlay);
    for (int y = 0; y < matrix.height(); y++) {
        char line[33];
        for (int x = 0; x < matrix.width(); x++) {
            const CRGB& c = matrixleds[matrix.XY(x, y)];
            uint8_t maxChannel = max(c.r, max(c.g, c.b));
            uint8_t threshold = maxChannel / 2;
            bool r = c.r > threshold, g = c.g > threshold, b = c.b > threshold;
            char ch;
            if (maxChannel == 0) ch = '.';
            else if (maxChannel < 16) ch = ':';
            else if (r && g && b) ch = 'W';
            else if (r && g) ch = 'Y';
            else if (r && b) ch = 'M';
            else if (g && b) ch = 'C';
            else if (r && c.g > maxChannel / 8) ch = 'A';
            else if (r) ch = 'R';
            else if (g) ch = 'G';
            else ch = 'B';
            line[x] = ch;
        }
        line[matrix.width()] = '\0';
        out.println(line);
    }
    if (overlayDrawn) restoreOverlay();
}

static const char *batteryLevelName(BatteryLevel level) {
    switch (level) {
        case BATTERY_UNKNOWN: return "UNKNOWN";
        case BATTERY_OK: return "OK";
        case BATTERY_LOW: return "LOW";
        case BATTERY_CRITICAL: return "CRITICAL";
    }
    return "?";
}

void Tc001::printBattery(Print& out) {
    out.printf("Battery: raw %u, median %u, level %s (low < %u, critical < %u)", batteryRaw, battery.filteredRaw(),
               batteryLevelName(battery.level()), BATTERY_RAW_LOW, BATTERY_RAW_CRITICAL);
    if (simulatedBatteryRaw >= 0) {
        out.printf(", simulated %d -> %s", (int)simulatedBatteryRaw, batteryLevelName(batteryLevel));
    }
    out.println();
}
#endif

bool Tc001::ensureConnection() {
    bool enforcePortal = digitalRead(PIN_MID_BUTTON) == LOW;   // if mid button pressed, enforce portal with no timeout
#ifdef BCP_DEV
    if (forcePortalFlag == FORCE_PORTAL_MAGIC) enforcePortal = true;
    forcePortalFlag = 0;
#endif
    bool retValue = WifiMgr.connect(enforcePortal, enforcePortal ? 0 : PORTAL_TIMEOUT_MS, this);
    if (retValue) {
        attachInterrupt(PIN_MID_BUTTON, []() { midButtonPressed = true; }, FALLING);
        attachInterrupt(PIN_RIGHT_BUTTON, []() { enforceUpdate = true; }, FALLING);
    }
    return retValue;
}

// Mid button pressed (debounced): start the config server and show its address
void Tc001::handleMidButton() {
    if (!midButtonPressed) return;
    static unsigned long lastPress = millis();
    unsigned long now = millis();
    if (now - lastPress < DEBOUNCE_DELAY_MS) return;
    lastPress = now;
    midButtonPressed = false;
    DisplayLock lock;
    stopAnimations();
    Config.startConfigServer();
    showConfigServerMsg("", false);
}

// Advances the animations and the countdown that are due, returns true if the matrix needs to be shown.
// Called with the display lock held.
bool Tc001::stepAnimations() {
    bool showMatrix = false;

    if (displayState == DISPLAY_CONFIG_MESSAGE && scrollingText && !scrollingText->isActive) {
        // config page address scrolled once, let main redraw the event data
        delete scrollingText;
        scrollingText = nullptr;
        redrawRequested = true;
    }

    if (scrollingText && scrollingText->isActive && (stMillis == 0 || millis() - stMillis > SCROLLING_TEXT_SPEED)) {
        stMillis = (stMillis == 0) ? millis() : stMillis + SCROLLING_TEXT_SPEED;
        scrollingText->step(true);
        showMatrix = true;
    }

    if (progressIndicator && progressIndicator->isActive && (piMillis == 0 || millis() - piMillis > PROGRESS_INDICATOR_SPEED)) {
        piMillis = (piMillis == 0) ? millis() : piMillis + PROGRESS_INDICATOR_SPEED;
        stepProgress();
        showMatrix = true;
    }

    // redraw the countdown once per second, right after the second changes
    if (displayState == DISPLAY_EVENT_COUNTDOWN && countdownEvent && time(nullptr) != countdownDrawnAt) {
        countdownDrawnAt = time(nullptr);
        drawCountdown(*countdownEvent);
        showMatrix = true;
    }

    // status indicator blinked or battery icon phase changed (also on a static screen)
    if (overlayState() != shownOverlay) {
        showMatrix = true;
    }

    return showMatrix;
}

void Tc001::handleEvents(void * parameter) {
    while (true) {
        handleMidButton();

        if (Config.isConfigServerRunning()) {
            Config.handleClient();
        }

        {
            DisplayLock lock;  // released before vTaskDelay()
            if (stepAnimations()) {
                showMatrix();
            }
        }

        stepBeep();
        stepBattery();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

size_t Tc001::write(uint8_t c) {
    if (c == '\n' || bufferIndex >= sizeof(buffer) - 1) {
        buffer[bufferIndex] = '\0';
        showMessage(buffer);
        bufferIndex = 0;
        return 1;
    }
    if (c != '\r' && bufferIndex < sizeof(buffer) - 2) {
        buffer[bufferIndex++] = c;
    }
    return 1;
}

// Error message in red, followed by the first message (what the device is doing) in white.
// Called with the display lock held.
void Tc001::showErrorMessage(const char *msg) {
    stopAnimations();
    matrix.clear();
    scrollingText = new ScrollingText(&matrix, msg, 0, 7, (const GFXfont *) defaultFont, false, COLOR_RED);
    if (initMsg_.length() > 0) {
        scrollingText->append(initMsg_, COLOR_WHITE, true);
    } else {
        errorShown_ = true;
    }
}

// Other message in white, appended after an error shown before the first message.
// Called with the display lock held.
void Tc001::showInfoMessage(const char *msg) {
    if (errorShown_) {
        scrollingText->append(msg, COLOR_WHITE, true);
        errorShown_ = false;
    } else {
        stopAnimations();
        matrix.clear();
        scrollingText = new ScrollingText(&matrix, msg, 0, 7, (const GFXfont *) defaultFont);
    }
    if (initMsg_.length() == 0) initMsg_ = String(msg);
}

// Shows one complete user message line, the line prefix decides how (see classifyMessage())
void Tc001::showMessage(const char *msg) {
    DisplayLock lock;
    switch (classifyMessage(msg)) {
        case MESSAGE_ERROR:
            showErrorMessage(msg);
            break;

        case MESSAGE_CONNECTING:
            // Connecting message, show progress indicator
            stopAnimations();
            matrix.clear();
            matrix.setCursor(2, 6);
            matrix.setTextColor(COLOR_WHITE);
            matrix.print("Connect");
            startProgress();
            break;

        case MESSAGE_CONNECTED:
            // do nothing, let the current status message continue
            break;

        default:
            showInfoMessage(msg);
            break;
    }
    showMatrix();
}
