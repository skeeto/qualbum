#include "srgb.hpp"

#include <algorithm>
#include <cmath>

namespace qualbum::srgb {

namespace {

// IEC 61966-2-1 sRGB transfer function. Input is the gamma-corrected display
// signal in [0, 1]; output is the linear-light value in [0, 1].
double srgb_decode(double s) {
    if (s <= 0.04045) return s / 12.92;
    return std::pow((s + 0.055) / 1.055, 2.4);
}

double srgb_encode(double l) {
    if (l <= 0.0031308) return 12.92 * l;
    return 1.055 * std::pow(l, 1.0 / 2.4) - 0.055;
}

std::array<float, 256> build_decode_lut() {
    std::array<float, 256> t{};
    for (int i = 0; i < 256; ++i) {
        t[static_cast<std::size_t>(i)] =
            static_cast<float>(srgb_decode(i / 255.0));
    }
    return t;
}

std::array<std::uint8_t, INVERSE_LUT_SIZE> build_encode_lut() {
    std::array<std::uint8_t, INVERSE_LUT_SIZE> t{};
    constexpr double scale = static_cast<double>(INVERSE_LUT_SIZE - 1);
    for (int i = 0; i < INVERSE_LUT_SIZE; ++i) {
        double linear = i / scale;
        double srgb01 = srgb_encode(linear);
        int v = static_cast<int>(std::lround(srgb01 * 255.0));
        v = std::clamp(v, 0, 255);
        t[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v);
    }
    return t;
}

}  // namespace

const std::array<float, 256>& srgb_to_linear() {
    static const auto t = build_decode_lut();
    return t;
}

const std::array<std::uint8_t, INVERSE_LUT_SIZE>& linear_to_srgb() {
    static const auto t = build_encode_lut();
    return t;
}

float linearize(std::uint8_t s) {
    return srgb_to_linear()[s];
}

std::uint8_t delinearize(float l) {
    float clamped = std::clamp(l, 0.0f, 1.0f);
    int idx = static_cast<int>(std::lround(
        clamped * static_cast<float>(INVERSE_LUT_SIZE - 1)));
    idx = std::clamp(idx, 0, INVERSE_LUT_SIZE - 1);
    return linear_to_srgb()[static_cast<std::size_t>(idx)];
}

}  // namespace qualbum::srgb
