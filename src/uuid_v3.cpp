#include "uuid_v3.hpp"

#include "md5.hpp"

#include <array>

namespace qualbum::uuidv3 {

namespace {

constexpr char hex_lc[] = "0123456789abcdef";

void hex_emit(char* out, std::uint8_t b) {
    out[0] = hex_lc[(b >> 4) & 0xfU];
    out[1] = hex_lc[b & 0xfU];
}

}  // namespace

std::string format(const Bytes& u) {
    std::string s;
    s.resize(36);
    char* p = s.data();
    static constexpr int groups[5] = {4, 2, 2, 2, 6};
    std::size_t bi = 0;
    bool first = true;
    for (int g : groups) {
        if (!first) {
            *p++ = '-';
        }
        for (int i = 0; i < g; ++i) {
            hex_emit(p, u[bi++]);
            p += 2;
        }
        first = false;
    }
    return s;
}

Bytes from_name(const Bytes& ns, std::string_view name) {
    md5::Hasher h;
    h.update(std::span<const std::uint8_t>(ns.data(), ns.size()));
    h.update(name);
    auto digest = h.finalize();
    Bytes out{};
    for (std::size_t i = 0; i < 16; ++i) out[i] = digest[i];
    // Version = 3 in high nibble of byte 6.
    out[6] = static_cast<std::uint8_t>((out[6] & 0x0fU) | 0x30U);
    // Variant 10x in top two bits of byte 8.
    out[8] = static_cast<std::uint8_t>((out[8] & 0x3fU) | 0x80U);
    return out;
}

}  // namespace qualbum::uuidv3
