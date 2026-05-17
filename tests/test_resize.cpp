#include "test_main.hpp"

#include "image_resize.hpp"
#include "srgb.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

using qualbum::image::RgbImage;
using qualbum::image::compute_average_rgb;
using qualbum::image::compute_gravity_crop;
using qualbum::image::crop;
using qualbum::image::resize_lanczos3;
using qualbum::image::make_thumbnail;
using qualbum::image::make_preview;
using qualbum::image::CropBox;

namespace {

RgbImage solid(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    RgbImage img;
    img.width = w;
    img.height = h;
    img.pixels.assign(static_cast<std::size_t>(w * h * 3), 0);
    for (int y = 0; y < h; ++y) {
        std::uint8_t* row = img.row(y);
        for (int x = 0; x < w; ++x) {
            row[3 * x + 0] = r;
            row[3 * x + 1] = g;
            row[3 * x + 2] = b;
        }
    }
    return img;
}

// 50% gray on the left half, white on the right half.
RgbImage half_split(int w, int h) {
    RgbImage img = solid(w, h, 255, 255, 255);
    for (int y = 0; y < h; ++y) {
        std::uint8_t* row = img.row(y);
        for (int x = 0; x < w / 2; ++x) {
            row[3 * x + 0] = 128;
            row[3 * x + 1] = 128;
            row[3 * x + 2] = 128;
        }
    }
    return img;
}

}  // namespace

TEST_CASE("crop: gravity 50 centers a portrait") {
    auto box = compute_gravity_crop(400, 600, 50);
    REQUIRE_EQ(box.x0, 0);
    REQUIRE_EQ(box.x1, 400);
    REQUIRE_EQ(box.y0, 100);
    REQUIRE_EQ(box.y1, 500);
}

TEST_CASE("crop: gravity 100 hugs the bottom of a portrait") {
    auto box = compute_gravity_crop(400, 600, 100);
    REQUIRE_EQ(box.y0, 200);
    REQUIRE_EQ(box.y1, 600);
}

TEST_CASE("crop: gravity 0 hugs the top of a portrait") {
    auto box = compute_gravity_crop(400, 600, 0);
    REQUIRE_EQ(box.y0, 0);
    REQUIRE_EQ(box.y1, 400);
}

TEST_CASE("crop: gravity on landscape (matches qualbum.py:125-130)") {
    auto box = compute_gravity_crop(800, 400, 50);
    REQUIRE_EQ(box.x0, 200);
    REQUIRE_EQ(box.x1, 600);
    REQUIRE_EQ(box.y0, 0);
    REQUIRE_EQ(box.y1, 400);
}

TEST_CASE("crop: extracts correct sub-rectangle") {
    auto img = half_split(100, 50);
    auto piece = crop(img, CropBox{0, 0, 50, 50});
    REQUIRE_EQ(piece.width, 50);
    REQUIRE_EQ(piece.height, 50);
    // top-left should be 50% gray
    REQUIRE_EQ(piece.pixels[0], 128);
}

TEST_CASE("resize: solid color stays solid (and equal value)") {
    auto src = solid(20, 20, 200, 100, 50);
    auto out = resize_lanczos3(src, 5, 5);
    REQUIRE_EQ(out.width, 5);
    REQUIRE_EQ(out.height, 5);
    for (std::size_t i = 0; i + 2 < out.pixels.size(); i += 3) {
        CHECK(std::abs(static_cast<int>(out.pixels[i + 0]) - 200) <= 1);
        CHECK(std::abs(static_cast<int>(out.pixels[i + 1]) - 100) <= 1);
        CHECK(std::abs(static_cast<int>(out.pixels[i + 2]) - 50) <= 1);
    }
}

TEST_CASE("resize: gamma-correct 50/50 gray+white averages above naive 191.5") {
    // A 50% sRGB-gray (128) and a 255 white averaged in *linear* light:
    //   Linear(128) ~ 0.2155; Linear(255) = 1.0; mean = 0.6077
    //   sRGB encoded mean = ~205.
    // A non-gamma-corrected naive resize would give ~192 (byte average).
    // So the gamma-correct output must be well above 192 and within a few
    // counts of the analytic 205.
    auto src = half_split(64, 8);
    auto out = resize_lanczos3(src, 1, 1);
    int luma = out.pixels[0];
    REQUIRE(luma >= 200);
    REQUIRE(luma <= 215);
}

TEST_CASE("resize_lanczos3: SIMD output is stable across sizes") {
    // Constructed gradient — predictable enough that the gamma-correct
    // mean is meaningful but with enough variation per pixel that
    // miscompiled SIMD lanes would distort the result.
    RgbImage src;
    src.width = 64;
    src.height = 64;
    src.pixels.assign(static_cast<std::size_t>(64 * 64 * 3), 0);
    for (int y = 0; y < 64; ++y) {
        std::uint8_t* row = src.row(y);
        for (int x = 0; x < 64; ++x) {
            row[3 * x + 0] = static_cast<std::uint8_t>((x * 4) & 0xff);
            row[3 * x + 1] = static_cast<std::uint8_t>((y * 4) & 0xff);
            row[3 * x + 2] = static_cast<std::uint8_t>(((x + y) * 2) & 0xff);
        }
    }
    auto out = resize_lanczos3(src, 16, 16);
    REQUIRE_EQ(out.width, 16);
    REQUIRE_EQ(out.height, 16);
    // Monotonic gradient axes survive the resize: top-left should be the
    // darkest red, bottom-right the brightest. (Rough check; exact values
    // depend on Lanczos weights so a couple of LSBs of tolerance is fine.)
    int tl_r = out.row(0)[0];
    int br_r = out.row(15)[3 * 15 + 0];
    REQUIRE(br_r > tl_r);
    int tl_g = out.row(0)[1];
    int br_g = out.row(15)[3 * 15 + 1];
    REQUIRE(br_g > tl_g);
}

TEST_CASE("make_thumbnail produces requested square") {
    auto src = solid(800, 600, 50, 150, 250);
    auto th = make_thumbnail(src, 50, 100);
    REQUIRE_EQ(th.width, 100);
    REQUIRE_EQ(th.height, 100);
}

TEST_CASE("compute_average_rgb: solid color round-trips") {
    auto img = solid(8, 8, 200, 100, 50);
    auto avg = compute_average_rgb(img);
    REQUIRE(std::abs(static_cast<int>(avg[0]) - 200) <= 1);
    REQUIRE(std::abs(static_cast<int>(avg[1]) - 100) <= 1);
    REQUIRE(std::abs(static_cast<int>(avg[2]) - 50)  <= 1);
}

TEST_CASE("compute_average_rgb: gamma-correct mean of 128 + 255") {
    // sRGB(128) ~= linear 0.2155; sRGB(255) = 1.0; mean = 0.6077.
    // Encoded back to sRGB ~= 205. A naive byte mean would give 191.
    auto img = half_split(64, 8);
    auto avg = compute_average_rgb(img);
    for (std::size_t c = 0; c < 3; ++c) {
        REQUIRE(avg[c] >= 200);
        REQUIRE(avg[c] <= 210);
    }
}

TEST_CASE("compute_average_rgb: empty image is black") {
    RgbImage empty;
    auto avg = compute_average_rgb(empty);
    REQUIRE_EQ(static_cast<int>(avg[0]), 0);
    REQUIRE_EQ(static_cast<int>(avg[1]), 0);
    REQUIRE_EQ(static_cast<int>(avg[2]), 0);
}

TEST_CASE("make_preview shrinks longest side and preserves aspect") {
    auto src = solid(2400, 1200, 10, 20, 30);
    auto pv = make_preview(src, 1200);
    REQUIRE_EQ(pv.width, 1200);
    REQUIRE_EQ(pv.height, 600);

    auto small = solid(800, 600, 10, 20, 30);
    auto kept = make_preview(small, 1200);
    REQUIRE_EQ(kept.width, 800);
    REQUIRE_EQ(kept.height, 600);
}
