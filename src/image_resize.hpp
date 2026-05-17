#pragma once

#include "image_io.hpp"

#include <array>
#include <cstdint>

namespace qualbum::image {

// Compute the gravity-aware square crop window for a source image.
// Mirrors the math in qualbum.py:119-136. Output coordinates are in
// pixels and are guaranteed to form a square inside the source rectangle.
struct CropBox {
    int x0, y0, x1, y1;
};
CropBox compute_gravity_crop(int src_w, int src_h, int gravity_pct);

// Gamma-correct separable Lanczos-3 resize. `dst_w` and `dst_h` must be > 0.
RgbImage resize_lanczos3(const RgbImage& src, int dst_w, int dst_h);

// Crop a sub-rectangle of `src`. The box must lie within the image bounds.
RgbImage crop(const RgbImage& src, const CropBox& box);

// Convenience: produce a square thumbnail of side `target` using gravity crop
// and gamma-correct Lanczos-3.
RgbImage make_thumbnail(const RgbImage& src, int gravity_pct, int target);

// Convenience: produce a preview such that the longer side equals `target`
// (or both sides if the source is already smaller, in which case the source
// is returned as-is — matching PIL's `thumbnail()` semantics).
RgbImage make_preview(const RgbImage& src, int target);

// Gamma-correct mean color of `img`. Each pixel is linearised through the
// sRGB transfer function, channels are summed and averaged in linear light,
// and the result is encoded back to sRGB. Suitable for the placeholder
// `background-color` on the single-photo page while the preview loads.
std::array<std::uint8_t, 3> compute_average_rgb(const RgbImage& img);

}  // namespace qualbum::image
