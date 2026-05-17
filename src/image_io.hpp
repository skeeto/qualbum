#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace qualbum::image {

struct RgbImage {
    int width = 0;
    int height = 0;
    static constexpr int channels = 3;
    std::vector<std::uint8_t> pixels;   // row-major, packed RGB

    std::size_t row_stride() const {
        return static_cast<std::size_t>(width) * 3;
    }
    std::uint8_t* row(int y) {
        return pixels.data()
             + static_cast<std::size_t>(y) * row_stride();
    }
    const std::uint8_t* row(int y) const {
        return pixels.data()
             + static_cast<std::size_t>(y) * row_stride();
    }
    bool empty() const { return width <= 0 || height <= 0; }
};

RgbImage decode_jpeg(const std::filesystem::path& path);
RgbImage decode_jpeg_mem(std::span<const std::uint8_t> data);

void encode_jpeg(const std::filesystem::path& path, const RgbImage& img,
                 int quality, bool optimize);
std::vector<std::uint8_t> encode_jpeg_mem(const RgbImage& img,
                                          int quality, bool optimize);

}  // namespace qualbum::image
