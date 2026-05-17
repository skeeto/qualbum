#pragma once

#include "datetime.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace qualbum::yamlx {

using Timestamp = dt::Timestamp;
using Scalar = std::variant<std::monostate, std::string, std::int64_t, Timestamp>;

class Map {
public:
    void set(std::string key, Scalar value);
    bool contains(std::string_view key) const;
    const Scalar* find(std::string_view key) const;

    // Coercing accessors. Return `def` if key is absent or null.
    std::string get_string(std::string_view key, std::string_view def = {}) const;
    std::int64_t get_int(std::string_view key, std::int64_t def = 0) const;
    std::optional<Timestamp> try_get_timestamp(std::string_view key) const;

    // Iterate insertion-ordered keys.
    const std::vector<std::string>& key_order() const { return order_; }

private:
    std::unordered_map<std::string, Scalar> kv_;
    std::vector<std::string> order_;
};

struct ParseError : public std::runtime_error {
    std::size_t line;
    ParseError(std::size_t l, std::string msg)
        : std::runtime_error(std::move(msg)), line(l) {}
};

// Parse a full YAML file. The format supported is a flat key:value map with
// scalar values: bare strings, double-quoted strings, integers, ISO 8601
// timestamps, or empty (null). Any other YAML construct raises ParseError.
Map parse_file(std::string_view text);

// Frontmatter: expects the first line to start with "---", parses key:value
// lines until the next line beginning with "---", returns the parsed map and
// the remaining text (the markdown body) verbatim.
struct Frontmatter {
    Map meta;
    std::string body;
};

Frontmatter parse_frontmatter(std::string_view text);

}  // namespace qualbum::yamlx
