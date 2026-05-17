// Micro-benchmark for image_resize. Decodes a source JPEG once, then runs
// make_thumbnail / make_preview repeatedly to time the scaling step in
// isolation from JPEG decode/encode.
//
// Output is a stable key=value block per mode so before/after runs can be
// diff'd line-for-line.

#include "image_io.hpp"
#include "image_resize.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

struct Args {
    fs::path image;
    int iters = 10;
    int warmup = 2;
    std::string mode = "both";  // thumb | preview | both
    int thumbsize = 300;
    int previewsize = 1200;
    int gravity = 50;
};

void print_help(const char* argv0) {
    std::printf(
        "Usage: %s [options]\n"
        "  --image PATH    Source JPEG (default: $QUALBUM_GALLERY_ROOT/birds/heron03.jpg)\n"
        "  --iters N       Timed iterations (default 10)\n"
        "  --warmup N      Untimed warmup iterations (default 2)\n"
        "  --mode M        thumb | preview | both (default both)\n"
        "  --thumbsize N   Thumbnail side (default 300)\n"
        "  --previewsize N Preview longer side (default 1200)\n"
        "  --gravity G     Thumbnail gravity 0..100 (default 50)\n",
        argv0);
}

Args parse_args(int argc, char** argv) {
    Args a;
    a.image = fs::path(QUALBUM_GALLERY_ROOT) / "birds" / "heron03.jpg";
    for (int i = 1; i < argc; ++i) {
        std::string_view k = argv[i];
        auto next = [&](const char* what) -> std::string_view {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (k == "-h" || k == "--help") { print_help(argv[0]); std::exit(0); }
        else if (k == "--image")        a.image = fs::path(std::string{next("--image")});
        else if (k == "--iters")        a.iters = std::atoi(std::string{next("--iters")}.c_str());
        else if (k == "--warmup")       a.warmup = std::atoi(std::string{next("--warmup")}.c_str());
        else if (k == "--mode")         a.mode = std::string{next("--mode")};
        else if (k == "--thumbsize")    a.thumbsize = std::atoi(std::string{next("--thumbsize")}.c_str());
        else if (k == "--previewsize")  a.previewsize = std::atoi(std::string{next("--previewsize")}.c_str());
        else if (k == "--gravity")      a.gravity = std::atoi(std::string{next("--gravity")}.c_str());
        else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            std::exit(2);
        }
    }
    return a;
}

struct Stats {
    double min_ms, median_ms, mean_ms, max_ms;
    double total_ms;
};

Stats summarise(std::vector<double>& samples_ms) {
    std::sort(samples_ms.begin(), samples_ms.end());
    Stats s{};
    s.min_ms    = samples_ms.front();
    s.max_ms    = samples_ms.back();
    s.median_ms = samples_ms[samples_ms.size() / 2];
    double sum = 0;
    for (double v : samples_ms) sum += v;
    s.mean_ms  = sum / static_cast<double>(samples_ms.size());
    s.total_ms = sum;
    return s;
}

void report(const char* label, const Stats& s, int out_pixels) {
    double megapix_per_sec = (out_pixels / 1.0e6) / (s.mean_ms / 1000.0);
    std::printf(
        "%s.min_ms    = %8.3f\n"
        "%s.median_ms = %8.3f\n"
        "%s.mean_ms   = %8.3f\n"
        "%s.max_ms    = %8.3f\n"
        "%s.mpx_per_s = %8.3f\n",
        label, s.min_ms,
        label, s.median_ms,
        label, s.mean_ms,
        label, s.max_ms,
        label, megapix_per_sec);
}

}  // namespace

int main(int argc, char** argv) {
    Args a = parse_args(argc, argv);
    if (!fs::exists(a.image)) {
        std::fprintf(stderr, "image not found: %s\n", a.image.string().c_str());
        return 1;
    }
    std::printf("image     = %s\n", a.image.string().c_str());
    std::printf("iters     = %d (warmup %d)\n", a.iters, a.warmup);

    auto src = qualbum::image::decode_jpeg(a.image);
    std::printf("source    = %dx%d (%.2f MP)\n",
                src.width, src.height,
                static_cast<double>(src.width) * src.height / 1.0e6);

    bool want_thumb = (a.mode == "thumb"   || a.mode == "both");
    bool want_prev  = (a.mode == "preview" || a.mode == "both");

    auto do_thumb = [&]() {
        return qualbum::image::make_thumbnail(src, a.gravity, a.thumbsize);
    };
    auto do_preview = [&]() {
        return qualbum::image::make_preview(src, a.previewsize);
    };

    if (want_thumb) {
        for (int i = 0; i < a.warmup; ++i) do_thumb();
        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>(a.iters));
        for (int i = 0; i < a.iters; ++i) {
            auto t0 = clk::now();
            auto out = do_thumb();
            auto t1 = clk::now();
            samples.push_back(
                std::chrono::duration<double, std::milli>(t1 - t0).count());
            asm volatile("" : : "r"(&out) : "memory");  // anti-DCE
        }
        auto s = summarise(samples);
        std::printf("\n[thumb %dx%d, gravity=%d]\n",
                    a.thumbsize, a.thumbsize, a.gravity);
        report("thumb", s, a.thumbsize * a.thumbsize);
    }

    if (want_prev) {
        for (int i = 0; i < a.warmup; ++i) do_preview();
        std::vector<double> samples;
        samples.reserve(static_cast<std::size_t>(a.iters));
        for (int i = 0; i < a.iters; ++i) {
            auto t0 = clk::now();
            auto out = do_preview();
            auto t1 = clk::now();
            samples.push_back(
                std::chrono::duration<double, std::milli>(t1 - t0).count());
            asm volatile("" : : "r"(&out) : "memory");
        }
        auto s = summarise(samples);
        // Output dimensions vary with source aspect; use longer-side² as a
        // rough megapixel approximation for the rate.
        int approx = a.previewsize * a.previewsize;
        std::printf("\n[preview longer-side<=%d]\n", a.previewsize);
        report("preview", s, approx);
    }

    return 0;
}
