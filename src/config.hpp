#pragma once

#include <filesystem>
#include <string>

namespace qualbum {

struct Config {
    std::string title{"Qualbum"};
    std::string author{"Nobody"};
    std::string baseurl{"http://example.com"};
    std::string prefix{};
    std::filesystem::path destination{"_site"};
    int thumbsize{300};
    int previewsize{1200};
    int pagemax{48};
    bool force{false};
    unsigned jobs{0};  // 0 -> hardware_concurrency()

    static Config load(const std::filesystem::path& yaml_file);
    static Config defaults() { return Config{}; }
};

}  // namespace qualbum
