#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace qualbum::uuidv3 {

using Bytes = std::array<std::uint8_t, 16>;

// RFC 4122 namespace UUIDs.
inline constexpr Bytes NAMESPACE_URL = {
    0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
    0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8,
};

// 8-4-4-4-12 lowercase hex representation of 16 raw bytes.
std::string format(const Bytes& uuid);

// MD5-based UUID v3 (RFC 4122 §4.3): concat namespace + name bytes, hash,
// then set version=3 and variant=10. Equivalent to Python uuid.uuid3.
Bytes from_name(const Bytes& namespace_uuid, std::string_view name);

inline std::string from_name_str(const Bytes& namespace_uuid,
                                 std::string_view name) {
    return format(from_name(namespace_uuid, name));
}

}  // namespace qualbum::uuidv3
