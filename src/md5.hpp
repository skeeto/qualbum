#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace qualbum::md5 {

using Digest = std::array<std::uint8_t, 16>;

class Hasher {
public:
    Hasher();
    void update(std::span<const std::uint8_t> data);
    void update(std::string_view sv);
    Digest finalize();

private:
    void transform(const std::uint8_t block[64]);

    std::uint32_t state_[4];
    std::uint64_t total_bytes_;
    std::uint8_t buffer_[64];
    std::size_t buf_used_;
};

Digest hash(std::span<const std::uint8_t> data);
Digest hash(std::string_view data);

}  // namespace qualbum::md5
