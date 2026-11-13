// On-device tests of the graphics helpers (lib/gfx): fonts, UTF-8 canvas, scrolling text, progress indicator
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <Arduino.h>
#include <unity.h>
#include <Adafruit_GFX.h>
#include <utf8canvas16.h>
#include <scrolling_text.h>
#include <progress_indicator.h>
#include <f3x5.h>
#include <f4x6.h>

void setUp() {}
void tearDown() {}

static const uint16_t WHITE = 0xFFFF;

// ---- Helpers ----

static bool sameBuffer(GFXcanvas16& a, GFXcanvas16& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    return memcmp(a.getBuffer(), b.getBuffer(), a.width() * a.height() * sizeof(uint16_t)) == 0;
}

static int litPixels(GFXcanvas16& c) {
    int n = 0;
    for (int i = 0; i < c.width() * c.height(); i++) {
        if (c.getBuffer()[i]) n++;
    }
    return n;
}

// Text rendered by UTF8canvas16 with the segmented F3x5 font, baseline y = 7
static void renderUtf8(UTF8canvas16& c, const char *text) {
    c.fillScreen(0);
    c.setFont(&F3x5[0], true);
    c.setTextColor(WHITE);
    c.setTextWrap(false);
    c.setCursor(0, 7);
    c.print(text);
}

// Single glyph rendered by plain GFXcanvas16 as the reference
static void renderGlyph(GFXcanvas16& c, int glyphIndex) {
    GFXfont f = { (uint8_t *) F3x5Bitmaps, (GFXglyph *) &F3x5Glyphs[glyphIndex], 'A', 'A', 6 };
    c.fillScreen(0);
    c.setFont(&f);
    c.setTextColor(WHITE);
    c.setTextWrap(false);
    c.setCursor(0, 7);
    c.print('A');
}

// Width of the scrolling text canvas, derived from the number of steps until a non-looping text ends
static int scrollWidth(const char *text) {
    GFXcanvas16 target(32, 8);
    ScrollingText s(&target, text, 0, 7, &F3x5[0], true, WHITE, 0, 0, false, false);
    int steps = 0;
    while (s.isActive && steps < 10000) {
        s.step(false);
        steps++;
    }
    return steps - 1;   // inactive when pos > totalWidth
}

// ---- Fonts ----

static void test_f3x5_segmentTable() {
    const size_t glyphCount = sizeof(F3x5Glyphs) / sizeof(F3x5Glyphs[0]);
    uint32_t prevLast = 0;
    size_t nextGlyph = 0;
    int i = 0;
    for (; i < 32 && F3x5[i].gfxFont.glyph != nullptr; i++) {
        const UTF8_32BitFont& seg = F3x5[i];
        uint32_t first = seg.dFirst ? seg.dFirst : seg.gfxFont.first;
        uint32_t last = seg.dLast ? seg.dLast : seg.gfxFont.last;
        char msg[48];
        snprintf(msg, sizeof(msg), "segment %d (U+%X..U+%X)", i, (unsigned) first, (unsigned) last);
        TEST_ASSERT_TRUE_MESSAGE(first <= last, msg);
        TEST_ASSERT_TRUE_MESSAGE(i == 0 || first > prevLast, msg);                   // ascending, no overlap
        TEST_ASSERT_TRUE_MESSAGE(last - first + 1 <= 256, msg);                      // glyph index is uint8_t in ScrollingText
        size_t glyphStart = seg.gfxFont.glyph - F3x5Glyphs;
        TEST_ASSERT_EQUAL_MESSAGE(nextGlyph, glyphStart, msg);                       // contiguous glyph table
        nextGlyph = glyphStart + (last - first + 1);
        TEST_ASSERT_TRUE_MESSAGE(nextGlyph <= glyphCount, msg);
        prevLast = last;
    }
    TEST_ASSERT_TRUE_MESSAGE(i < 32, "missing terminator");
    TEST_ASSERT_EQUAL(glyphCount, nextGlyph);
}

static void assertGlyphBitmapsInRange(const GFXglyph *glyphs, size_t count, size_t bitmapSize, const char *font) {
    for (size_t i = 0; i < count; i++) {
        size_t bytes = (glyphs[i].width * glyphs[i].height + 7) / 8;
        char msg[48];
        snprintf(msg, sizeof(msg), "%s glyph %u", font, (unsigned) i);
        TEST_ASSERT_TRUE_MESSAGE(glyphs[i].bitmapOffset + bytes <= bitmapSize, msg);
    }
}

static void test_fonts_bitmapsInRange() {
    assertGlyphBitmapsInRange(F3x5Glyphs, sizeof(F3x5Glyphs) / sizeof(F3x5Glyphs[0]), sizeof(F3x5Bitmaps), "F3x5");
    assertGlyphBitmapsInRange(F3x5Glyphs_Fixed, sizeof(F3x5Glyphs_Fixed) / sizeof(F3x5Glyphs_Fixed[0]), sizeof(F3x5Bitmaps), "F3x5_Fixed");
    assertGlyphBitmapsInRange(F4x6Glyphs, sizeof(F4x6Glyphs) / sizeof(F4x6Glyphs[0]), sizeof(F4x6Bitmaps), "F4x6");
}

static void test_timeFonts_coverDashToColon() {
    TEST_ASSERT_EQUAL('-', F4x6.first);
    TEST_ASSERT_EQUAL(':', F4x6.last);
    TEST_ASSERT_EQUAL('-', F3x5_Fixed.first);
    TEST_ASSERT_EQUAL(':', F3x5_Fixed.last);
    TEST_ASSERT_EQUAL(':' - '-' + 1, sizeof(F4x6Glyphs) / sizeof(F4x6Glyphs[0]));
    TEST_ASSERT_EQUAL(':' - '-' + 1, sizeof(F3x5Glyphs_Fixed) / sizeof(F3x5Glyphs_Fixed[0]));
}

// ---- UTF8canvas16 ----

static void test_utf8Canvas_glyphsOfAllSegments() {
    struct { const char *text; int glyph; } cases[] = {
        { "A", 'A' - 32 },
        { "\xC5\x98", 280 },          // U+0158 Ř (2 bytes)
        { "\xC5\xBE", 318 },          // U+017E ž
        { "\xD0\x96", 326 },          // U+0416 Ж
        { "\xF0\x9F\x87\xA8", 486 },  // U+1F1E8 regional indicator C (4 bytes)
    };
    UTF8canvas16 c(16, 8);
    GFXcanvas16 ref(16, 8);
    for (auto& tc : cases) {
        renderUtf8(c, tc.text);
        renderGlyph(ref, tc.glyph);
        TEST_ASSERT_TRUE_MESSAGE(litPixels(ref) > 0, tc.text);
        TEST_ASSERT_TRUE_MESSAGE(sameBuffer(c, ref), tc.text);
    }
}

static void test_utf8Canvas_invalidAndUnknownSkipped() {
    UTF8canvas16 c(32, 8), ref(32, 8);
    renderUtf8(ref, "AB");
    const char *variants[] = {
        "A\x80" "B",                  // lone continuation byte
        "A\xFF" "B",                  // invalid byte
        "A\xF0\x9F\x98\x80" "B",      // U+1F600 emoji, not in the font
    };
    for (const char *v : variants) {
        renderUtf8(c, v);
        TEST_ASSERT_TRUE_MESSAGE(sameBuffer(c, ref), v);
    }
}

// ---- ScrollingText ----

// The scroll canvas width must match the rendered text, also for bytes / characters the canvas skips
static void test_scrollingText_widthIgnoresSkippedChars() {
    struct { const char *text, *clean; } cases[] = {
        { "A\xF0\x9F\x98\x80" "B", "AB" },   // emoji not in the font
        { "A\x80" "B", "AB" },               // lone continuation byte
        { "A\xFF" "B", "AB" },               // invalid byte
    };
    for (auto& tc : cases) {
        TEST_ASSERT_EQUAL_MESSAGE(scrollWidth(tc.clean), scrollWidth(tc.text), tc.text);
    }
}

// First frame (pos 0, lpad 0) equals the text rendered directly: nothing clipped
static void test_scrollingText_firstFrameNotClipped() {
    const char *texts[] = { "Hello", "P\xC5\x99\xC3\xAD" "li\xC5\xA1 \xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD", "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", "1!$" };
    for (const char *t : texts) {
        GFXcanvas16 target(128, 8);
        target.fillScreen(0);
        ScrollingText s(&target, t, 0, 7, &F3x5[0], true, WHITE, 0, 0, false, true);
        UTF8canvas16 ref(128, 8);
        renderUtf8(ref, t);
        TEST_ASSERT_TRUE_MESSAGE(sameBuffer(target, ref), t);
    }
}

// Appended text uses the same font handling as the initial text (non-ASCII included)
static void test_scrollingText_appendNonAscii() {
    GFXcanvas16 a(128, 8), b(128, 8);
    a.fillScreen(0);
    b.fillScreen(0);
    ScrollingText whole(&a, "A\xC5\xBE", 0, 7, &F3x5[0], true, WHITE, 0, 0, false, true);
    ScrollingText parts(&b, "A", 0, 7, &F3x5[0], true, WHITE, 0, 0, false, false);
    parts.append("\xC5\xBE", WHITE, false, 1, 0);   // append joins at the ink edge: lpad 1 = gap after "A"
    parts.draw();
    TEST_ASSERT_TRUE(sameBuffer(a, b));
}

static void test_scrollingText_nonLoopingEnds() {
    int w = scrollWidth("Hello");
    TEST_ASSERT_TRUE(w > 10);
    GFXcanvas16 target(32, 8);
    ScrollingText s(&target, "Hello", 0, 7, &F3x5[0], true, WHITE, 32, 0, false, false);
    for (int i = 0; i < 32 + w; i++) {
        s.step(false);
        TEST_ASSERT_TRUE(s.isActive);
    }
    s.step(false);
    TEST_ASSERT_FALSE(s.isActive);
}

// Looping text with an appended part (newStart): every step shifts the frame by exactly one pixel,
// also across the restart (regression: the first part flashed at the restart)
static void test_scrollingText_loopIsSeamless() {
    GFXcanvas16 target(32, 8), prev(32, 8);
    target.fillScreen(0);
    ScrollingText s(&target, "Error: X", 0, 7, &F3x5[0], true, 0xF800, 32, 0, true, true);
    s.append("Config http://1.2.3.4", WHITE, true);
    for (int i = 0; i < 400; i++) {
        memcpy(prev.getBuffer(), target.getBuffer(), 32 * 8 * sizeof(uint16_t));
        s.step(true);
        TEST_ASSERT_TRUE(s.isActive);
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 31; x++) {
                if (target.getPixel(x, y) != prev.getPixel(x + 1, y)) {
                    char msg[48];
                    snprintf(msg, sizeof(msg), "step %d pixel %d,%d", i, x, y);
                    TEST_FAIL_MESSAGE(msg);
                }
            }
        }
    }
}

// ---- ProgressIndicator ----

static void test_progressIndicator_staysWithinColumns() {
    GFXcanvas16 c(32, 1);
    c.fillScreen(0);
    ProgressIndicator p(&c, 0, 0, 32, WHITE, 0);
    p.draw();
    int minX = 32, maxX = -1;
    for (int i = 0; i < 100; i++) {
        p.step(true);
        TEST_ASSERT_EQUAL(1, litPixels(c));
        for (int x = 0; x < 32; x++) {
            if (c.getPixel(x, 0)) {
                minX = min(minX, x);
                maxX = max(maxX, x);
            }
        }
    }
    TEST_ASSERT_EQUAL(0, minX);
    TEST_ASSERT_EQUAL(31, maxX);
    p.hide();
    TEST_ASSERT_EQUAL(0, litPixels(c));
    TEST_ASSERT_FALSE(p.isActive);
}

void setup() {
    delay(2000);   // the serial port reset the board, give the test runner time to attach
    UNITY_BEGIN();
    RUN_TEST(test_f3x5_segmentTable);
    RUN_TEST(test_fonts_bitmapsInRange);
    RUN_TEST(test_timeFonts_coverDashToColon);
    RUN_TEST(test_utf8Canvas_glyphsOfAllSegments);
    RUN_TEST(test_utf8Canvas_invalidAndUnknownSkipped);
    RUN_TEST(test_scrollingText_widthIgnoresSkippedChars);
    RUN_TEST(test_scrollingText_firstFrameNotClipped);
    RUN_TEST(test_scrollingText_appendNonAscii);
    RUN_TEST(test_scrollingText_nonLoopingEnds);
    RUN_TEST(test_scrollingText_loopIsSeamless);
    RUN_TEST(test_progressIndicator_staysWithinColumns);
    UNITY_END();
}

void loop() {}
