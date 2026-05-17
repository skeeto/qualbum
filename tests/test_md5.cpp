#include "test_main.hpp"

#include "md5.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

std::string to_hex(const qualbum::md5::Digest& d) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string s;
    s.resize(32);
    for (std::size_t i = 0; i < 16; ++i) {
        s[2 * i]     = hex[(d[i] >> 4) & 0xf];
        s[2 * i + 1] = hex[d[i] & 0xf];
    }
    return s;
}

}  // namespace

TEST_CASE("md5: RFC 1321 vectors") {
    using qualbum::md5::hash;
    REQUIRE_EQ(to_hex(hash(std::string_view{""})), std::string{"d41d8cd98f00b204e9800998ecf8427e"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"a"})), std::string{"0cc175b9c0f1b6a831c399e269772661"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"abc"})), std::string{"900150983cd24fb0d6963f7d28e17f72"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"message digest"})),
               std::string{"f96b697d7cb7938d525a2f31aaf161d0"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"abcdefghijklmnopqrstuvwxyz"})),
               std::string{"c3fcd3d76192e4007dfb496cca67e13b"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"})),
               std::string{"d174ab98d277d9f5a5611c2c9f419d9f"});
    REQUIRE_EQ(to_hex(hash(std::string_view{"12345678901234567890123456789012345678901234567890123456789012345678901234567890"})),
               std::string{"57edf4a22be3c955ac49da2e2107b67a"});
}

TEST_CASE("md5: incremental update matches one-shot") {
    using namespace qualbum::md5;
    const std::string msg(1024, 'X');
    Hasher one;
    one.update(msg);
    auto a = to_hex(one.finalize());

    Hasher many;
    std::size_t chunks[] = {1, 3, 17, 63, 64, 65, 128, 256, 511};
    std::size_t off = 0;
    std::size_t i = 0;
    while (off < msg.size()) {
        std::size_t take = chunks[i++ % (sizeof chunks / sizeof chunks[0])];
        if (off + take > msg.size()) take = msg.size() - off;
        many.update(std::string_view(msg.data() + off, take));
        off += take;
    }
    auto b = to_hex(many.finalize());
    REQUIRE_EQ(a, b);
}

TEST_CASE("md5: 64-byte boundary") {
    using namespace qualbum::md5;
    std::string msg(64, 'k');
    REQUIRE_EQ(to_hex(hash(msg)), to_hex(hash(std::string_view(msg))));
    // Independent reference: 64 'k' chars -> known hash via Python:
    // python3 -c "import hashlib; print(hashlib.md5(b'k'*64).hexdigest())"
    REQUIRE_EQ(to_hex(hash(msg)), std::string{"a18cc771b8188ff945d0dd7757c50fd1"});
}
