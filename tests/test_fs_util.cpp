#include "test_main.hpp"

#include "fs_util.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir(const std::string& prefix) {
    auto base = fs::temp_directory_path() / "qualbum_tests";
    fs::create_directories(base);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned> dist(0, 0xffffffff);
    for (int attempt = 0; attempt < 100; ++attempt) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%08x", dist(gen));
        auto p = base / (prefix + "-" + buf);
        if (fs::create_directory(p)) return p;
    }
    throw std::runtime_error("could not create temp dir");
}

void write_file(const fs::path& p, std::string_view text) {
    std::ofstream f(p, std::ios::binary);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

}  // namespace

TEST_CASE("fs_util: route_to_path strips leading slash") {
    using qualbum::fsu::route_to_path;
    REQUIRE_EQ(route_to_path("_site", "/space/moon01/thumb.jpg").generic_string(),
               std::string{"_site/space/moon01/thumb.jpg"});
    REQUIRE_EQ(route_to_path("_site", "/").generic_string(),
               std::string{"_site"});
    REQUIRE_EQ(route_to_path("_site", "/albums/").generic_string(),
               std::string{"_site/albums/"});
    REQUIRE_EQ(route_to_path("_out", "no-slash/idx.html").generic_string(),
               std::string{"_out/no-slash/idx.html"});
}

TEST_CASE("fs_util: mkdir_p idempotent and creates parents") {
    auto tmp = make_temp_dir("mkdir");
    auto deep = tmp / "a" / "b" / "c" / "d";
    qualbum::fsu::mkdir_p(deep);
    REQUIRE(fs::is_directory(deep));
    qualbum::fsu::mkdir_p(deep);  // again — no throw
    REQUIRE(fs::is_directory(deep));
    fs::remove_all(tmp);
}

TEST_CASE("fs_util: hard_link replaces existing target") {
    auto tmp = make_temp_dir("link");
    auto src = tmp / "src.txt";
    auto dst = tmp / "dst.txt";
    write_file(src, "hello");
    write_file(dst, "stale");
    qualbum::fsu::hard_link(src, dst);
    std::ifstream f(dst, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
    REQUIRE_EQ(body, std::string{"hello"});
    fs::remove_all(tmp);
}

TEST_CASE("fs_util: newer reports correctly") {
    using qualbum::fsu::newer;
    auto tmp = make_temp_dir("newer");
    auto a = tmp / "a.txt";
    auto b = tmp / "b.txt";
    write_file(a, "a");
    REQUIRE(newer(a, b));   // b does not exist
    write_file(b, "b");
    // Bump a's mtime to be strictly later than b.
    auto bt = fs::last_write_time(b);
    fs::last_write_time(a, bt + std::chrono::seconds(2));
    REQUIRE(newer(a, b));
    fs::last_write_time(a, bt - std::chrono::seconds(2));
    REQUIRE(!newer(a, b));
    fs::remove_all(tmp);
}
