#include "md5.hpp"

#include <bit>
#include <cstring>

namespace qualbum::md5 {

namespace {

constexpr std::uint32_t K[64] = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
    0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
    0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
    0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,

    0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
    0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
    0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
    0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,

    0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
    0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
    0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,

    0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
    0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
    0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
};

constexpr std::uint32_t S[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

inline std::uint32_t load_le_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void store_le_u32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v & 0xffU);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xffU);
    p[2] = static_cast<std::uint8_t>((v >> 16) & 0xffU);
    p[3] = static_cast<std::uint8_t>((v >> 24) & 0xffU);
}

}  // namespace

Hasher::Hasher() : total_bytes_(0), buf_used_(0) {
    state_[0] = 0x67452301U;
    state_[1] = 0xefcdab89U;
    state_[2] = 0x98badcfeU;
    state_[3] = 0x10325476U;
}

void Hasher::transform(const std::uint8_t block[64]) {
    std::uint32_t M[16];
    for (int i = 0; i < 16; ++i) {
        M[i] = load_le_u32(block + i * 4);
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];

    for (int i = 0; i < 64; ++i) {
        std::uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        std::uint32_t temp = d;
        d = c;
        c = b;
        b = b + std::rotl(a + f + K[i] + M[g], static_cast<int>(S[i]));
        a = temp;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}

void Hasher::update(std::span<const std::uint8_t> data) {
    const std::uint8_t* p = data.data();
    std::size_t n = data.size();
    total_bytes_ += n;

    if (buf_used_) {
        std::size_t want = 64 - buf_used_;
        std::size_t take = n < want ? n : want;
        std::memcpy(buffer_ + buf_used_, p, take);
        buf_used_ += take;
        p += take;
        n -= take;
        if (buf_used_ == 64) {
            transform(buffer_);
            buf_used_ = 0;
        }
    }
    while (n >= 64) {
        transform(p);
        p += 64;
        n -= 64;
    }
    if (n) {
        std::memcpy(buffer_, p, n);
        buf_used_ = n;
    }
}

void Hasher::update(std::string_view sv) {
    update(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size()));
}

Digest Hasher::finalize() {
    std::uint64_t bit_len = total_bytes_ * 8;

    buffer_[buf_used_++] = 0x80U;
    if (buf_used_ > 56) {
        while (buf_used_ < 64) buffer_[buf_used_++] = 0;
        transform(buffer_);
        buf_used_ = 0;
    }
    while (buf_used_ < 56) buffer_[buf_used_++] = 0;

    for (int i = 0; i < 8; ++i) {
        buffer_[56 + i] = static_cast<std::uint8_t>((bit_len >> (8 * i)) & 0xffU);
    }
    transform(buffer_);

    Digest out;
    for (int i = 0; i < 4; ++i) {
        store_le_u32(out.data() + i * 4, state_[i]);
    }
    return out;
}

Digest hash(std::span<const std::uint8_t> data) {
    Hasher h;
    h.update(data);
    return h.finalize();
}

Digest hash(std::string_view data) {
    Hasher h;
    h.update(data);
    return h.finalize();
}

}  // namespace qualbum::md5
