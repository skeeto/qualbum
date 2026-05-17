#include "test_main.hpp"

#include "datetime.hpp"

#include <stdexcept>

using qualbum::dt::parse_iso8601;
using qualbum::dt::try_parse_iso8601;
using qualbum::dt::to_iso8601;
using qualbum::dt::to_iso8601_with_trailing_z;
using qualbum::dt::format_display;
using qualbum::dt::earlier;

TEST_CASE("datetime: parse with negative offset round-trips") {
    auto t = parse_iso8601("2017-10-07T19:38:00-04:00");
    REQUIRE_EQ(t.year, 2017);
    REQUIRE_EQ(t.month, 10);
    REQUIRE_EQ(t.day, 7);
    REQUIRE_EQ(t.hour, 19);
    REQUIRE_EQ(t.minute, 38);
    REQUIRE_EQ(t.second, 0);
    REQUIRE_EQ(t.tz_offset_min, -240);
    REQUIRE(t.has_time);
    REQUIRE(!t.had_z_marker);
    REQUIRE_EQ(to_iso8601(t), std::string{"2017-10-07T19:38:00-04:00"});
    REQUIRE_EQ(to_iso8601_with_trailing_z(t),
               std::string{"2017-10-07T19:38:00-04:00Z"});
}

TEST_CASE("datetime: Z marker preserved and emits +00:00 (Python parity)") {
    auto t = parse_iso8601("2017-07-28T00:00:00Z");
    REQUIRE_EQ(t.tz_offset_min, 0);
    REQUIRE(t.had_z_marker);
    // PyYAML parses 'Z' into a UTC datetime whose isoformat() emits "+00:00".
    REQUIRE_EQ(to_iso8601(t), std::string{"2017-07-28T00:00:00+00:00"});
}

TEST_CASE("datetime: positive offset and zero-padding") {
    auto t = parse_iso8601("2017-09-27T17:19:19+00:00");
    REQUIRE_EQ(t.tz_offset_min, 0);
    REQUIRE(!t.had_z_marker);
    REQUIRE_EQ(to_iso8601(t), std::string{"2017-09-27T17:19:19+00:00"});

    auto t2 = parse_iso8601("2024-10-12T12:29:00-04:00");
    REQUIRE_EQ(t2.year, 2024);
    REQUIRE_EQ(to_iso8601_with_trailing_z(t2),
               std::string{"2024-10-12T12:29:00-04:00Z"});
}

TEST_CASE("datetime: display format matches strftime %B %d, %Y") {
    auto t = parse_iso8601("2017-10-07T19:38:00-04:00");
    REQUIRE_EQ(format_display(t), std::string{"October 07, 2017"});
    auto t2 = parse_iso8601("2025-02-18T08:00:00Z");
    REQUIRE_EQ(format_display(t2), std::string{"February 18, 2025"});
    auto t3 = parse_iso8601("2008-07-09T00:00:00Z");
    REQUIRE_EQ(format_display(t3), std::string{"July 09, 2008"});
}

TEST_CASE("datetime: ordering across timezones") {
    auto a = parse_iso8601("2017-10-07T15:00:00-04:00");  // 19:00 UTC
    auto b = parse_iso8601("2017-10-07T20:00:00+00:00");  // 20:00 UTC
    REQUIRE(earlier(a, b));
    REQUIRE(!earlier(b, a));
    REQUIRE(!earlier(a, a));
}

TEST_CASE("datetime: malformed inputs reject") {
    REQUIRE(!try_parse_iso8601("not a date").has_value());
    REQUIRE(!try_parse_iso8601("2017-13-01T00:00:00Z").has_value());
    REQUIRE(!try_parse_iso8601("2017-10-07T19:38").has_value());
    REQUIRE_THROWS(parse_iso8601("blah"));
}

TEST_CASE("datetime: optional fractional seconds accepted") {
    auto t = parse_iso8601("2017-10-07T19:38:00.123-04:00");
    REQUIRE_EQ(t.second, 0);
    REQUIRE_EQ(t.tz_offset_min, -240);
}
