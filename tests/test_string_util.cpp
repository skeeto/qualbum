#include "test_main.hpp"

#include "string_util.hpp"

using qualbum::strutil::html_escape;
using qualbum::strutil::xml_escape;
using qualbum::strutil::strip;
using qualbum::strutil::starts_with;
using qualbum::strutil::ends_with;
using qualbum::strutil::lower_ascii;

TEST_CASE("html_escape: text mode preserves quotes") {
    REQUIRE_EQ(html_escape("plain"), std::string{"plain"});
    REQUIRE_EQ(html_escape("a&b<c>d\"e"), std::string{"a&amp;b&lt;c&gt;d\"e"});
    REQUIRE_EQ(html_escape("&amp;"), std::string{"&amp;amp;"});
}

TEST_CASE("html_escape: attr mode escapes quotes too") {
    REQUIRE_EQ(html_escape("\"hi\"", true), std::string{"&quot;hi&quot;"});
    REQUIRE_EQ(html_escape("a & b", true), std::string{"a &amp; b"});
}

TEST_CASE("xml_escape: covers all five entities") {
    REQUIRE_EQ(xml_escape("<a href=\"x'y\">&</a>"),
               std::string{"&lt;a href=&quot;x&apos;y&quot;&gt;&amp;&lt;/a&gt;"});
}

TEST_CASE("strip handles whitespace classes") {
    REQUIRE_EQ(strip("  hi  "), std::string_view{"hi"});
    REQUIRE_EQ(strip("\t\nhi\r\n"), std::string_view{"hi"});
    REQUIRE_EQ(strip(""), std::string_view{""});
    REQUIRE_EQ(strip("   "), std::string_view{""});
}

TEST_CASE("starts_with / ends_with") {
    REQUIRE(starts_with("foobar", "foo"));
    REQUIRE(!starts_with("foobar", "bar"));
    REQUIRE(ends_with("foobar", "bar"));
    REQUIRE(!ends_with("foobar", "foo"));
    REQUIRE(starts_with("x", ""));
    REQUIRE(ends_with("x", ""));
}

TEST_CASE("lower_ascii leaves non-ASCII alone") {
    REQUIRE_EQ(lower_ascii("HELLO"), std::string{"hello"});
    REQUIRE_EQ(lower_ascii("AbC123"), std::string{"abc123"});
    REQUIRE_EQ(lower_ascii("Héllo"), std::string{"h\xc3\xa9llo"});
}
