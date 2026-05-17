#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace qualbum::dt {

// ISO 8601 timestamp with timezone offset preserved.
// `tz_offset_min` is signed minutes east of UTC. `Z` parses to 0 (with
// `had_z_marker` true so the round-trip uses 'Z' instead of '+00:00').
struct Timestamp {
    int year{};
    int month{};
    int day{};
    int hour{};
    int minute{};
    int second{};
    int tz_offset_min{};
    bool has_time{false};
    bool had_z_marker{false};
};

// Parses ISO 8601 of the forms used in the corpus:
//   YYYY-MM-DD
//   YYYY-MM-DDTHH:MM:SS
//   YYYY-MM-DDTHH:MM:SSZ
//   YYYY-MM-DDTHH:MM:SS[+-]HH:MM
// Throws std::runtime_error on malformed input.
Timestamp parse_iso8601(std::string_view text);
std::optional<Timestamp> try_parse_iso8601(std::string_view text);

// Python datetime.isoformat() compatible. With time: includes offset as
// "+HH:MM"/"-HH:MM" or the 'Z' marker if `had_z_marker` is set.
std::string to_iso8601(const Timestamp& t);

// Matches Python's `iso + 'Z'` idiom used in qualbum.py:102, :188.
std::string to_iso8601_with_trailing_z(const Timestamp& t);

// strftime("%B %d, %Y") — e.g. "October 07, 2017".
std::string format_display(const Timestamp& t);

// True iff `a` is strictly earlier than `b` (compares in wall-clock UTC).
bool earlier(const Timestamp& a, const Timestamp& b);

// "Now" in the system's local timezone (mirrors Python's
// `datetime.now(timezone.utc).astimezone()`).
Timestamp now_local();

}  // namespace qualbum::dt
