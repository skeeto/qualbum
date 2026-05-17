#pragma once

#include <array>
#include <cstdint>

namespace qualbum::srgb {

// Number of slots in the inverse LUT. 8192 is the smallest power of two
// that yields exact 8-bit round-trip across all 256 inputs.
inline constexpr int INVERSE_LUT_SIZE = 8192;

// 256-entry table: sRGB byte -> linear value in [0, 1].
const std::array<float, 256>& srgb_to_linear();

// INVERSE_LUT_SIZE-entry table: linear in [0, 1] -> sRGB byte.
// Index is `clamp(round(linear * (INVERSE_LUT_SIZE - 1)), 0, INVERSE_LUT_SIZE - 1)`.
const std::array<std::uint8_t, INVERSE_LUT_SIZE>& linear_to_srgb();

// Scalar helpers (mostly for tests; the hot path uses the LUTs directly).
float linearize(std::uint8_t s);
std::uint8_t delinearize(float l);

}  // namespace qualbum::srgb
