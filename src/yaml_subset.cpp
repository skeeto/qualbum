#include "yaml_subset.hpp"

#include "string_util.hpp"

#include <cctype>
#include <stdexcept>

namespace qualbum::yamlx {

void Map::set(std::string key, Scalar value) {
    auto it = kv_.find(key);
    if (it == kv_.end()) {
        order_.push_back(key);
    }
    kv_[std::move(key)] = std::move(value);
}

bool Map::contains(std::string_view key) const {
    return kv_.find(std::string{key}) != kv_.end();
}

const Scalar* Map::find(std::string_view key) const {
    auto it = kv_.find(std::string{key});
    if (it == kv_.end()) return nullptr;
    return &it->second;
}

std::string Map::get_string(std::string_view key, std::string_view def) const {
    auto* p = find(key);
    if (!p) return std::string{def};
    if (std::holds_alternative<std::string>(*p)) return std::get<std::string>(*p);
    if (std::holds_alternative<std::int64_t>(*p))
        return std::to_string(std::get<std::int64_t>(*p));
    if (std::holds_alternative<Timestamp>(*p))
        return dt::to_iso8601(std::get<Timestamp>(*p));
    return std::string{def};
}

std::int64_t Map::get_int(std::string_view key, std::int64_t def) const {
    auto* p = find(key);
    if (!p) return def;
    if (std::holds_alternative<std::int64_t>(*p))
        return std::get<std::int64_t>(*p);
    return def;
}

std::optional<Timestamp> Map::try_get_timestamp(std::string_view key) const {
    auto* p = find(key);
    if (!p) return std::nullopt;
    if (std::holds_alternative<Timestamp>(*p))
        return std::get<Timestamp>(*p);
    if (std::holds_alternative<std::string>(*p))
        return dt::try_parse_iso8601(std::get<std::string>(*p));
    return std::nullopt;
}

namespace {

bool is_ws(char c) { return c == ' ' || c == '\t'; }

// Returns true and consumes the value, false if it doesn't look like a
// timestamp.
bool looks_like_timestamp(std::string_view sv) {
    // YYYY-MM-DD ...
    if (sv.size() < 10) return false;
    auto is_d = [](char c) { return c >= '0' && c <= '9'; };
    return is_d(sv[0]) && is_d(sv[1]) && is_d(sv[2]) && is_d(sv[3])
        && sv[4] == '-'
        && is_d(sv[5]) && is_d(sv[6])
        && sv[7] == '-'
        && is_d(sv[8]) && is_d(sv[9]);
}

bool looks_like_int(std::string_view sv) {
    if (sv.empty()) return false;
    std::size_t i = 0;
    if (sv[0] == '-' || sv[0] == '+') i = 1;
    if (i == sv.size()) return false;
    for (; i < sv.size(); ++i) {
        if (sv[i] < '0' || sv[i] > '9') return false;
    }
    return true;
}

bool is_bare_key_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

std::string parse_double_quoted(std::string_view in, std::size_t lineno) {
    // in starts with " ... must end with " (after escapes)
    std::string out;
    out.reserve(in.size());
    std::size_t i = 1;
    bool closed = false;
    while (i < in.size()) {
        char c = in[i];
        if (c == '"') {
            closed = true;
            ++i;
            break;
        }
        if (c == '\\' && i + 1 < in.size()) {
            char e = in[i + 1];
            switch (e) {
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '0':  out.push_back('\0'); break;
                default:
                    throw ParseError(lineno,
                        "unsupported escape \\" + std::string(1, e));
            }
            i += 2;
        } else {
            out.push_back(c);
            ++i;
        }
    }
    if (!closed) throw ParseError(lineno, "unterminated double-quoted string");
    // After the closing quote, only whitespace is allowed.
    while (i < in.size()) {
        if (!is_ws(in[i]))
            throw ParseError(lineno, "trailing junk after quoted string");
        ++i;
    }
    return out;
}

void reject_unsupported_leading(std::string_view trimmed, std::size_t lineno) {
    if (trimmed.empty()) return;
    switch (trimmed.front()) {
        case '-':
            if (trimmed.size() >= 2 && (trimmed[1] == ' ' || trimmed[1] == '\t')) {
                throw ParseError(lineno, "block lists not supported");
            }
            break;
        case '[':
        case '{':
            throw ParseError(lineno, "flow collections not supported");
        case '*':
        case '&':
            throw ParseError(lineno, "anchors/aliases not supported");
        case '|':
        case '>':
            throw ParseError(lineno, "block scalars not supported");
        case '?':
            throw ParseError(lineno, "explicit keys not supported");
        default:
            break;
    }
}

Scalar classify_value(std::string_view trimmed, std::size_t lineno) {
    if (trimmed.empty()) return std::monostate{};

    if (trimmed.front() == '"') {
        return parse_double_quoted(trimmed, lineno);
    }
    if (trimmed.front() == '\'') {
        throw ParseError(lineno, "single-quoted strings not supported");
    }
    if (trimmed.front() == '[' || trimmed.front() == '{') {
        throw ParseError(lineno, "flow collections not supported");
    }
    if (trimmed.front() == '&' || trimmed.front() == '*') {
        throw ParseError(lineno, "anchors/aliases not supported");
    }
    if (trimmed.front() == '|' || trimmed.front() == '>') {
        throw ParseError(lineno, "block scalars not supported");
    }

    if (looks_like_timestamp(trimmed)) {
        auto t = dt::try_parse_iso8601(trimmed);
        if (t) return *t;
        // Falls through to bare string if not a valid timestamp.
    }
    if (looks_like_int(trimmed)) {
        try {
            return static_cast<std::int64_t>(std::stoll(std::string{trimmed}));
        } catch (...) {
            // Falls through.
        }
    }
    // Bare string. (Note: 'iso: ISO100' is a bare string, not an int — the
    // alphabetic prefix prevents int classification.)
    return std::string{trimmed};
}

void parse_line(Map& m, std::string_view line, std::size_t lineno) {
    // Reject indentation (we only support flat top-level maps).
    if (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
        // Allow purely-whitespace blank lines.
        bool any_non_ws = false;
        for (char c : line) {
            if (!is_ws(c)) { any_non_ws = true; break; }
        }
        if (!any_non_ws) return;
        throw ParseError(lineno, "indented lines not supported");
    }
    if (line.empty()) return;
    if (line.front() == '#') return;  // (corpus has no comments, but tolerate)

    reject_unsupported_leading(line, lineno);

    // Parse bare key.
    std::size_t i = 0;
    while (i < line.size() && is_bare_key_char(line[i])) ++i;
    if (i == 0) {
        throw ParseError(lineno, "expected key character at start of line");
    }
    std::string key{line.substr(0, i)};
    while (i < line.size() && is_ws(line[i])) ++i;
    if (i == line.size() || line[i] != ':') {
        throw ParseError(lineno, "expected ':' after key '" + key + "'");
    }
    ++i;  // skip ':'
    auto rest = line.substr(i);
    rest = strutil::strip(rest);
    Scalar val = classify_value(rest, lineno);
    m.set(std::move(key), std::move(val));
}

}  // namespace

Map parse_file(std::string_view text) {
    Map m;
    std::size_t lineno = 0;
    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t j = i;
        while (j < text.size() && text[j] != '\n') ++j;
        auto line = text.substr(i, j - i);
        // Trim CR if present.
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        ++lineno;
        parse_line(m, line, lineno);
        if (j == text.size()) break;
        i = j + 1;
    }
    return m;
}

Frontmatter parse_frontmatter(std::string_view text) {
    // Find first line.
    std::size_t i = 0;
    auto next_line = [&](std::size_t& start, std::size_t& end_excl) -> bool {
        if (start > text.size()) return false;
        std::size_t j = start;
        while (j < text.size() && text[j] != '\n') ++j;
        end_excl = j;
        return true;
    };

    // First line must start with "---" (may have trailing CR).
    std::size_t start = 0, end = 0;
    if (!next_line(start, end)) {
        throw ParseError(0, "frontmatter missing");
    }
    auto first = text.substr(start, end - start);
    if (!first.empty() && first.back() == '\r') first.remove_suffix(1);
    if (!strutil::starts_with(first, "---")) {
        throw ParseError(1, "frontmatter must begin with '---'");
    }

    i = (end < text.size()) ? end + 1 : text.size();
    std::size_t lineno = 1;

    Map m;
    while (i <= text.size()) {
        ++lineno;
        std::size_t s = i;
        std::size_t e = 0;
        if (!next_line(s, e)) break;
        auto line = text.substr(s, e - s);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (strutil::starts_with(line, "---")) {
            // End of frontmatter. Body is everything after this line.
            std::size_t body_start = (e < text.size()) ? e + 1 : text.size();
            return Frontmatter{std::move(m), std::string{text.substr(body_start)}};
        }
        parse_line(m, line, lineno);
        if (e == text.size()) break;
        i = e + 1;
    }
    throw ParseError(lineno, "frontmatter not terminated by closing '---'");
}

}  // namespace qualbum::yamlx
