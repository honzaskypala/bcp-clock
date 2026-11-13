// Unit tests of the shared web page helpers (lib/webui)
// (c) 2025 Honza Skýpala
// WTFPL license applies

#include <unity.h>
#include <Arduino.h>
#include <webui.h>

void setUp() {}
void tearDown() {}

static void test_htmlEscape_specialChars() {
    TEST_ASSERT_EQUAL_STRING("&amp;&lt;&gt;&quot;&#39;", WebUI::htmlEscape("&<>\"'").c_str());
}

static void test_htmlEscape_plainAndUtf8Unchanged() {
    TEST_ASSERT_EQUAL_STRING("Kolo 3 – Řež", WebUI::htmlEscape("Kolo 3 – Řež").c_str());
    TEST_ASSERT_EQUAL_STRING("", WebUI::htmlEscape("").c_str());
}

static void test_htmlEscape_mixed() {
    TEST_ASSERT_EQUAL_STRING("a&lt;b&gt;c&amp;d", WebUI::htmlEscape("a<b>c&d").c_str());
}

static void test_pageStart_escapesTitle() {
    String page = WebUI::pageStart("<x>");
    TEST_ASSERT_TRUE(page.indexOf("<title>&lt;x&gt;</title>") >= 0);
    TEST_ASSERT_TRUE(page.indexOf("<h1>&lt;x&gt;</h1>") >= 0);
    TEST_ASSERT_EQUAL(-1, page.indexOf("<x>"));
}

static void test_sectionHeader_firstClass() {
    TEST_ASSERT_TRUE(WebUI::sectionHeader("A", "", true).startsWith("<div class='secHeader first'>"));
    TEST_ASSERT_TRUE(WebUI::sectionHeader("A").startsWith("<div class='secHeader'>"));
    TEST_ASSERT_TRUE(WebUI::sectionHeader("A&B", "<b>btn</b>").indexOf("<h2>A&amp;B</h2><b>btn</b></div>") >= 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_htmlEscape_specialChars);
    RUN_TEST(test_htmlEscape_plainAndUtf8Unchanged);
    RUN_TEST(test_htmlEscape_mixed);
    RUN_TEST(test_pageStart_escapesTitle);
    RUN_TEST(test_sectionHeader_firstClass);
    return UNITY_END();
}
