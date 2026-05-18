#include "image_io.hpp"

#include "fs_util.hpp"

#include <cassert>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include <jpeglib.h>

namespace qualbum::image {

namespace {

template <typename To, typename From>
constexpr To narrow_cast(From v) {
    auto out = static_cast<To>(v);
    assert(static_cast<From>(out) == v);
    return out;
}

struct ErrorMgr {
    struct jpeg_error_mgr pub;
    std::jmp_buf jmpbuf;
    char msg[JMSG_LENGTH_MAX];
};

void on_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<ErrorMgr*>(cinfo->err);
    err->pub.format_message(cinfo, err->msg);
    std::longjmp(err->jmpbuf, 1);
}

std::vector<std::uint8_t> read_all(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open image: " + path.string());
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    std::vector<std::uint8_t> data;
    if (sz > 0) {
        data.resize(static_cast<std::size_t>(sz));
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(data.data()), sz);
        if (!f) throw std::runtime_error("image read failed: " + path.string());
    }
    return data;
}

void write_all(const std::filesystem::path& path,
               std::span<const std::uint8_t> data) {
    fsu::mkdir_p(path.parent_path());
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw std::runtime_error("cannot write image: " + path.string());
        if (!data.empty()) {
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(data.size()));
        }
        if (!f) throw std::runtime_error("image write failed: " + path.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
        if (ec) throw std::runtime_error(
            "rename failed for image: " + path.string());
    }
}

}  // namespace

RgbImage decode_jpeg_mem(std::span<const std::uint8_t> data) {
    struct jpeg_decompress_struct cinfo {};
    ErrorMgr jerr {};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = on_error_exit;

    if (setjmp(jerr.jmpbuf)) {
        std::string msg = jerr.msg;
        jpeg_destroy_decompress(&cinfo);
        throw std::runtime_error("jpeg decode failed: " + msg);
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data.data(), narrow_cast<unsigned long>(data.size()));
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    RgbImage img;
    img.width = static_cast<int>(cinfo.output_width);
    img.height = static_cast<int>(cinfo.output_height);
    img.pixels.resize(static_cast<std::size_t>(img.width)
                      * static_cast<std::size_t>(img.height) * 3);

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row_ptr = img.row(static_cast<int>(cinfo.output_scanline));
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return img;
}

RgbImage decode_jpeg(const std::filesystem::path& path) {
    auto bytes = read_all(path);
    return decode_jpeg_mem(bytes);
}

std::vector<std::uint8_t> encode_jpeg_mem(const RgbImage& img,
                                          int quality, bool optimize) {
    if (img.empty()) throw std::runtime_error("encode_jpeg: empty image");

    struct jpeg_compress_struct cinfo {};
    ErrorMgr jerr {};
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = on_error_exit;

    unsigned char* outbuf = nullptr;
    unsigned long outsize = 0;

    if (setjmp(jerr.jmpbuf)) {
        std::string msg = jerr.msg;
        if (outbuf) std::free(outbuf);
        jpeg_destroy_compress(&cinfo);
        throw std::runtime_error("jpeg encode failed: " + msg);
    }

    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuf, &outsize);

    cinfo.image_width = static_cast<JDIMENSION>(img.width);
    cinfo.image_height = static_cast<JDIMENSION>(img.height);
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    cinfo.optimize_coding = optimize ? TRUE : FALSE;
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_ptr = const_cast<JSAMPROW>(
            img.row(static_cast<int>(cinfo.next_scanline)));
        jpeg_write_scanlines(&cinfo, &row_ptr, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    std::vector<std::uint8_t> result(outbuf, outbuf + outsize);
    std::free(outbuf);
    return result;
}

void encode_jpeg(const std::filesystem::path& path, const RgbImage& img,
                 int quality, bool optimize) {
    auto bytes = encode_jpeg_mem(img, quality, optimize);
    write_all(path, bytes);
}

}  // namespace qualbum::image
