#include "test_main.hpp"

#include "srgb.hpp"

#include <algorithm>
#include <cmath>

TEST_CASE("srgb: 256-input round-trip is exact") {
    using namespace qualbum::srgb;
    int worst_drift = 0;
    for (int v = 0; v < 256; ++v) {
        float lin = linearize(static_cast<std::uint8_t>(v));
        std::uint8_t back = delinearize(lin);
        int drift = std::abs(static_cast<int>(back) - v);
        worst_drift = std::max(worst_drift, drift);
        CHECK_EQ(static_cast<int>(back), v);
    }
    REQUIRE_EQ(worst_drift, 0);
}

TEST_CASE("srgb: linearize(0)=0, linearize(255)=1") {
    using namespace qualbum::srgb;
    REQUIRE_EQ(linearize(0), 0.0f);
    REQUIRE_EQ(linearize(255), 1.0f);
}

TEST_CASE("srgb: monotonic in both directions") {
    using namespace qualbum::srgb;
    float prev = -1.0f;
    for (int v = 0; v < 256; ++v) {
        float cur = linearize(static_cast<std::uint8_t>(v));
        REQUIRE(cur >= prev);
        prev = cur;
    }
    std::uint8_t prev_u = 0;
    for (int i = 0; i < INVERSE_LUT_SIZE; ++i) {
        float lin = static_cast<float>(i) / static_cast<float>(INVERSE_LUT_SIZE - 1);
        std::uint8_t cur = delinearize(lin);
        REQUIRE(cur >= prev_u);
        prev_u = cur;
    }
}

TEST_CASE("srgb: linear mid-gray sanity") {
    using namespace qualbum::srgb;
    // sRGB 128 should map to about 0.2159 linear; 0.5 linear should encode
    // to about sRGB 188.
    float lin128 = linearize(128);
    REQUIRE(lin128 > 0.214f);
    REQUIRE(lin128 < 0.218f);
    std::uint8_t s_half = delinearize(0.5f);
    REQUIRE(s_half >= 186);
    REQUIRE(s_half <= 190);
}
