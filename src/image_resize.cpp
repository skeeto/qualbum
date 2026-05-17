#include "image_resize.hpp"

#include "srgb.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace qualbum::image {

namespace {

// Portable 4×float SIMD vector via the GCC `vector_size` extension.
// Compiles to NEON (Pi 4 / aarch64) or SSE (x86-64) without any
// platform-specific intrinsics; on hosts with no matching ISA the compiler
// emits scalar code. We carry pixels as padded RGBA (the fourth lane is a
// zero scratch slot) so each filter tap is a single broadcast-multiply-add
// over an aligned 16-byte vector.
typedef float v4f __attribute__((vector_size(16)));

constexpr int LANCZOS_A = 3;

float sinc(float x) {
    if (x == 0.0f) return 1.0f;
    float px = std::numbers::pi_v<float> * x;
    return std::sin(px) / px;
}

float lanczos_kernel(float x) {
    float ax = std::fabs(x);
    if (ax >= static_cast<float>(LANCZOS_A)) return 0.0f;
    return sinc(x) * sinc(x / static_cast<float>(LANCZOS_A));
}

// Precomputed per-output-pixel filter description.
struct Filter {
    int start;                  // first source pixel index (clamped later)
    std::vector<float> weights; // weights[k] applies to source index start + k
};

std::vector<Filter> build_filter(int src_n, int dst_n) {
    assert(src_n > 0 && dst_n > 0);
    std::vector<Filter> filters(static_cast<std::size_t>(dst_n));
    float scale = static_cast<float>(src_n) / static_cast<float>(dst_n);
    float inv_scale = 1.0f / scale;
    // When downsampling, stretch the kernel. When upsampling, kernel stays
    // at its native width.
    float filter_scale = std::max(scale, 1.0f);
    float support = static_cast<float>(LANCZOS_A) * filter_scale;

    for (int j = 0; j < dst_n; ++j) {
        float center = (static_cast<float>(j) + 0.5f) * scale - 0.5f;
        int left = static_cast<int>(std::floor(center - support + 1.0f));
        int right = static_cast<int>(std::floor(center + support));
        Filter& f = filters[static_cast<std::size_t>(j)];
        f.start = left;
        int count = right - left + 1;
        if (count < 1) count = 1;
        f.weights.resize(static_cast<std::size_t>(count));
        float sum = 0.0f;
        for (int k = 0; k < count; ++k) {
            float src_idx = static_cast<float>(left + k);
            float t = (src_idx - center) / filter_scale;
            float w;
            if (filter_scale > 1.0f) {
                // Already stretched; normalize.
                w = sinc(t) * sinc(t / static_cast<float>(LANCZOS_A));
                if (std::fabs(t) >= static_cast<float>(LANCZOS_A)) w = 0.0f;
                w *= inv_scale;  // energy normalization
            } else {
                w = lanczos_kernel(t);
            }
            f.weights[static_cast<std::size_t>(k)] = w;
            sum += w;
        }
        if (sum != 0.0f) {
            for (auto& w : f.weights) w /= sum;
        }
    }
    return filters;
}

inline int clamp_idx(int v, int max_excl) {
    if (v < 0) return 0;
    if (v >= max_excl) return max_excl - 1;
    return v;
}

}  // namespace

CropBox compute_gravity_crop(int src_w, int src_h, int gravity_pct) {
    CropBox box{};
    int gravity = gravity_pct * std::max(src_w, src_h) / 100;
    if (src_w > src_h) {
        int maxx = src_w - src_h;
        int x0 = std::min(std::max(gravity - src_h / 2, 0), maxx);
        box.x0 = x0;
        box.x1 = src_h + x0;
        box.y0 = 0;
        box.y1 = src_h;
    } else {
        int maxy = src_h - src_w;
        int y0 = std::min(std::max(gravity - src_w / 2, 0), maxy);
        box.y0 = y0;
        box.y1 = src_w + y0;
        box.x0 = 0;
        box.x1 = src_w;
    }
    return box;
}

RgbImage crop(const RgbImage& src, const CropBox& box) {
    if (box.x0 < 0 || box.y0 < 0
        || box.x1 > src.width || box.y1 > src.height
        || box.x0 >= box.x1 || box.y0 >= box.y1) {
        throw std::runtime_error("crop box out of bounds");
    }
    RgbImage out;
    out.width = box.x1 - box.x0;
    out.height = box.y1 - box.y0;
    out.pixels.resize(static_cast<std::size_t>(out.width)
                      * static_cast<std::size_t>(out.height) * 3);
    const std::size_t row_bytes = static_cast<std::size_t>(out.width) * 3;
    for (int y = 0; y < out.height; ++y) {
        const std::uint8_t* src_row = src.row(box.y0 + y)
            + static_cast<std::size_t>(box.x0) * 3;
        std::memcpy(out.row(y), src_row, row_bytes);
    }
    return out;
}

RgbImage resize_lanczos3(const RgbImage& src, int dst_w, int dst_h) {
    if (dst_w <= 0 || dst_h <= 0) throw std::runtime_error("invalid target size");
    if (src.empty()) throw std::runtime_error("resize: empty source");

    const auto& fwd = srgb::srgb_to_linear();
    const auto& inv = srgb::linear_to_srgb();
    constexpr float lut_max = static_cast<float>(srgb::INVERSE_LUT_SIZE - 1);

    const int src_w = src.width;
    const int src_h = src.height;

    // Fuse the linearize and horizontal passes so we never materialize the
    // full src_h*src_w linsrc buffer. For a 24 MP source that buffer was
    // 384 MB — bigger than Pi 4 L2 and most of the resize wall time was
    // spent shuttling it through main memory. Row-by-row linearization
    // keeps a single 16-byte-aligned scratch row resident in L2, and we
    // pay source DRAM traffic only once.
    auto fx = build_filter(src_w, dst_w);
    std::vector<v4f> mid(
        static_cast<std::size_t>(src_h)
        * static_cast<std::size_t>(dst_w));
    std::vector<v4f> row_lin(static_cast<std::size_t>(src_w));

    for (int y = 0; y < src_h; ++y) {
        const std::uint8_t* sr = src.row(y);
        for (int x = 0; x < src_w; ++x) {
            row_lin[static_cast<std::size_t>(x)] =
                v4f{fwd[sr[3 * x + 0]],
                    fwd[sr[3 * x + 1]],
                    fwd[sr[3 * x + 2]],
                    0.0f};
        }
        v4f* dr = &mid[static_cast<std::size_t>(y)
                       * static_cast<std::size_t>(dst_w)];
        for (int x = 0; x < dst_w; ++x) {
            const Filter& f = fx[static_cast<std::size_t>(x)];
            v4f acc{};
            const int n_taps = static_cast<int>(f.weights.size());
            for (int k = 0; k < n_taps; ++k) {
                int sx = clamp_idx(f.start + k, src_w);
                v4f ws = {f.weights[static_cast<std::size_t>(k)],
                          f.weights[static_cast<std::size_t>(k)],
                          f.weights[static_cast<std::size_t>(k)],
                          f.weights[static_cast<std::size_t>(k)]};
                acc += ws * row_lin[static_cast<std::size_t>(sx)];
            }
            dr[x] = acc;
        }
    }

    // Vertical pass + encode: (src_h, dst_w) v4f -> (dst_h, dst_w) packed
    // RGB bytes. Encode is per-channel scalar (LUT gather doesn't benefit
    // from the SIMD width).
    auto fy = build_filter(src_h, dst_h);
    RgbImage out;
    out.width = dst_w;
    out.height = dst_h;
    out.pixels.resize(static_cast<std::size_t>(dst_w)
                      * static_cast<std::size_t>(dst_h) * 3);
    for (int y = 0; y < dst_h; ++y) {
        const Filter& f = fy[static_cast<std::size_t>(y)];
        std::uint8_t* dr = out.row(y);
        const int n_taps = static_cast<int>(f.weights.size());
        for (int x = 0; x < dst_w; ++x) {
            v4f acc{};
            for (int k = 0; k < n_taps; ++k) {
                int sy = clamp_idx(f.start + k, src_h);
                const v4f* sr = &mid[static_cast<std::size_t>(sy)
                                     * static_cast<std::size_t>(dst_w)];
                float w = f.weights[static_cast<std::size_t>(k)];
                v4f ws = {w, w, w, w};
                acc += ws * sr[x];
            }
            auto encode = [&](float v) -> std::uint8_t {
                v = std::clamp(v, 0.0f, 1.0f);
                int idx = static_cast<int>(std::lround(v * lut_max));
                if (idx < 0) idx = 0;
                if (idx >= srgb::INVERSE_LUT_SIZE) idx = srgb::INVERSE_LUT_SIZE - 1;
                return inv[static_cast<std::size_t>(idx)];
            };
            dr[3 * x + 0] = encode(acc[0]);
            dr[3 * x + 1] = encode(acc[1]);
            dr[3 * x + 2] = encode(acc[2]);
        }
    }
    return out;
}

RgbImage make_thumbnail(const RgbImage& src, int gravity_pct, int target) {
    CropBox box = compute_gravity_crop(src.width, src.height, gravity_pct);
    RgbImage square = crop(src, box);
    return resize_lanczos3(square, target, target);
}

RgbImage make_preview(const RgbImage& src, int target) {
    int w = src.width, h = src.height;
    int dst_w = w, dst_h = h;
    if (w > target || h > target) {
        if (w >= h) {
            dst_w = target;
            dst_h = std::max(1, static_cast<int>(
                std::lround(static_cast<double>(h) * target / w)));
        } else {
            dst_h = target;
            dst_w = std::max(1, static_cast<int>(
                std::lround(static_cast<double>(w) * target / h)));
        }
        return resize_lanczos3(src, dst_w, dst_h);
    }
    // PIL `thumbnail` is a no-op when both dimensions are already within
    // the target box. Mirror that.
    return src;
}

std::array<std::uint8_t, 3> compute_average_rgb(const RgbImage& img) {
    if (img.empty()) return {0, 0, 0};
    const auto& fwd = srgb::srgb_to_linear();
    const auto& inv = srgb::linear_to_srgb();
    constexpr float lut_max = static_cast<float>(srgb::INVERSE_LUT_SIZE - 1);

    double sum_r = 0, sum_g = 0, sum_b = 0;
    const std::size_t n_pixels =
        static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height);
    const std::uint8_t* p = img.pixels.data();
    for (std::size_t i = 0; i < n_pixels; ++i) {
        sum_r += static_cast<double>(fwd[p[3 * i + 0]]);
        sum_g += static_cast<double>(fwd[p[3 * i + 1]]);
        sum_b += static_cast<double>(fwd[p[3 * i + 2]]);
    }
    auto encode = [&](double v) -> std::uint8_t {
        double clamped = std::clamp(v / static_cast<double>(n_pixels), 0.0, 1.0);
        int idx = static_cast<int>(std::lround(clamped * static_cast<double>(lut_max)));
        if (idx < 0) idx = 0;
        if (idx >= srgb::INVERSE_LUT_SIZE) idx = srgb::INVERSE_LUT_SIZE - 1;
        return inv[static_cast<std::size_t>(idx)];
    };
    return {encode(sum_r), encode(sum_g), encode(sum_b)};
}

}  // namespace qualbum::image
