#include "datetime.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <stdexcept>

namespace qualbum::dt {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool read_digits(std::string_view& sv, int n, int& out) {
    if (sv.size() < static_cast<std::size_t>(n)) return false;
    int v = 0;
    for (int i = 0; i < n; ++i) {
        char c = sv[static_cast<std::size_t>(i)];
        if (!is_digit(c)) return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    sv.remove_prefix(static_cast<std::size_t>(n));
    return true;
}

bool consume(std::string_view& sv, char c) {
    if (sv.empty() || sv.front() != c) return false;
    sv.remove_prefix(1);
    return true;
}

constexpr std::array<const char*, 12> MONTH_NAMES = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

// Days from the proleptic Gregorian epoch (year 0). Used for ordering.
long long days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153U * static_cast<unsigned>(m > 2 ? m - 3 : m + 9) + 2U) / 5U
                 + static_cast<unsigned>(d) - 1U;
    unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return static_cast<long long>(era) * 146097LL
        + static_cast<long long>(doe) - 719468LL;
}

}  // namespace

std::optional<Timestamp> try_parse_iso8601(std::string_view text) {
    Timestamp t;
    std::string_view sv = text;
    if (!read_digits(sv, 4, t.year)) return std::nullopt;
    if (!consume(sv, '-')) return std::nullopt;
    if (!read_digits(sv, 2, t.month)) return std::nullopt;
    if (!consume(sv, '-')) return std::nullopt;
    if (!read_digits(sv, 2, t.day)) return std::nullopt;
    if (t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31) return std::nullopt;

    if (sv.empty()) return t;

    char tsep = sv.front();
    if (tsep == 'T' || tsep == 't' || tsep == ' ') {
        sv.remove_prefix(1);
        if (!read_digits(sv, 2, t.hour)) return std::nullopt;
        if (!consume(sv, ':')) return std::nullopt;
        if (!read_digits(sv, 2, t.minute)) return std::nullopt;
        if (!consume(sv, ':')) return std::nullopt;
        if (!read_digits(sv, 2, t.second)) return std::nullopt;
        t.has_time = true;
        // Optional fractional seconds — accepted and discarded.
        if (!sv.empty() && sv.front() == '.') {
            sv.remove_prefix(1);
            while (!sv.empty() && is_digit(sv.front())) sv.remove_prefix(1);
        }
        // Timezone.
        if (!sv.empty()) {
            char c = sv.front();
            if (c == 'Z' || c == 'z') {
                sv.remove_prefix(1);
                t.tz_offset_min = 0;
                t.had_z_marker = true;
            } else if (c == '+' || c == '-') {
                int sign = (c == '+') ? 1 : -1;
                sv.remove_prefix(1);
                int hh = 0, mm = 0;
                if (!read_digits(sv, 2, hh)) return std::nullopt;
                // Optional colon, then minutes.
                if (!sv.empty() && sv.front() == ':') sv.remove_prefix(1);
                if (!sv.empty() && is_digit(sv.front())) {
                    if (!read_digits(sv, 2, mm)) return std::nullopt;
                }
                t.tz_offset_min = sign * (hh * 60 + mm);
            }
        }
    }
    if (!sv.empty()) return std::nullopt;
    return t;
}

Timestamp parse_iso8601(std::string_view text) {
    if (auto r = try_parse_iso8601(text)) return *r;
    throw std::runtime_error("invalid ISO 8601 timestamp: " + std::string(text));
}

std::string to_iso8601(const Timestamp& t) {
    char buf[64];
    if (!t.has_time) {
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", t.year, t.month, t.day);
        return std::string(buf);
    }
    if (t.had_z_marker) {
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                      t.year, t.month, t.day, t.hour, t.minute, t.second);
        return std::string(buf);
    }
    int off = t.tz_offset_min;
    char sign = off < 0 ? '-' : '+';
    int absoff = off < 0 ? -off : off;
    std::snprintf(buf, sizeof buf,
                  "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                  t.year, t.month, t.day, t.hour, t.minute, t.second,
                  sign, absoff / 60, absoff % 60);
    return std::string(buf);
}

std::string to_iso8601_with_trailing_z(const Timestamp& t) {
    return to_iso8601(t) + "Z";
}

std::string format_display(const Timestamp& t) {
    char buf[64];
    const char* month_name = (t.month >= 1 && t.month <= 12)
        ? MONTH_NAMES[static_cast<std::size_t>(t.month - 1)]
        : "?";
    std::snprintf(buf, sizeof buf, "%s %02d, %d", month_name, t.day, t.year);
    return std::string(buf);
}

bool earlier(const Timestamp& a, const Timestamp& b) {
    auto to_utc_secs = [](const Timestamp& t) -> long long {
        long long d = days_from_civil(t.year, t.month, t.day);
        long long secs = d * 86400LL;
        if (t.has_time) {
            secs += static_cast<long long>(t.hour) * 3600LL
                  + static_cast<long long>(t.minute) * 60LL
                  + static_cast<long long>(t.second);
            secs -= static_cast<long long>(t.tz_offset_min) * 60LL;
        }
        return secs;
    };
    return to_utc_secs(a) < to_utc_secs(b);
}

Timestamp now_local() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    Timestamp t;
    t.year = local.tm_year + 1900;
    t.month = local.tm_mon + 1;
    t.day = local.tm_mday;
    t.hour = local.tm_hour;
    t.minute = local.tm_min;
    t.second = local.tm_sec;
    t.has_time = true;
#if defined(_WIN32)
    // GetTimeZoneInformation gives the offset for the current local time.
    long bias_min = 0;
    // _get_timezone returns seconds west of UTC, ignoring DST.
    // We'd want DST-aware. Simpler: use diff between gmtime and localtime.
    std::tm gm{};
    gmtime_s(&gm, &now);
    std::time_t local_as_utc = _mkgmtime(&local);
    std::time_t gm_as_utc = _mkgmtime(&gm);
    bias_min = static_cast<long>((local_as_utc - gm_as_utc) / 60);
    t.tz_offset_min = static_cast<int>(bias_min);
#else
    t.tz_offset_min = static_cast<int>(local.tm_gmtoff / 60);
#endif
    return t;
}

}  // namespace qualbum::dt
