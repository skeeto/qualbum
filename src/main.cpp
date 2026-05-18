#include "app.hpp"
#include "config.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

void print_help(const char* argv0) {
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "Generates a static photo gallery in the current directory using\n"
        "_config.yaml, _gallery.yaml, _gallery.html, _single.html, and _feed.xml.\n"
        "\n"
        "Options:\n"
        "  --destination DIR  Output directory (default from _config.yaml)\n"
        "  --jobs N           Resize-worker thread count (default: hardware concurrency)\n"
        "  --force            Rebuild all thumbnails/previews regardless of mtime\n"
        "  --config FILE      Config YAML path (default: _config.yaml)\n"
        "  --version, -V      Print version and exit\n"
        "  --help, -h         Show this message\n",
        argv0);
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path config_path = "_config.yaml";
    std::string destination_override;
    unsigned jobs_override = 0;
    bool force_override = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        auto needs_arg = [&](std::string_view) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", argv[i]);
                std::exit(2);
            }
            return std::string_view{argv[++i]};
        };
        if (a == "-h" || a == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (a == "-V" || a == "--version") {
            std::printf("qualbum %s\n", QUALBUM_VERSION);
            return 0;
        } else if (a == "--destination" || a == "-d") {
            destination_override = needs_arg(a);
        } else if (a.starts_with("--destination=")) {
            destination_override = a.substr(std::strlen("--destination="));
        } else if (a == "--jobs" || a == "-j") {
            jobs_override = static_cast<unsigned>(std::atoi(std::string{needs_arg(a)}.c_str()));
        } else if (a.starts_with("--jobs=")) {
            jobs_override = static_cast<unsigned>(std::atoi(std::string{a.substr(std::strlen("--jobs="))}.c_str()));
        } else if (a == "--force" || a == "-f") {
            force_override = true;
        } else if (a == "--config") {
            config_path = std::filesystem::path(std::string{needs_arg(a)});
        } else if (a.starts_with("--config=")) {
            config_path = std::filesystem::path(std::string{a.substr(std::strlen("--config="))});
        } else {
            std::fprintf(stderr, "unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 2;
        }
    }

    try {
        qualbum::Config cfg = qualbum::Config::load(config_path);
        if (!destination_override.empty()) {
            cfg.destination = destination_override;
        }
        if (jobs_override > 0) {
            cfg.jobs = jobs_override;
        }
        if (force_override) {
            cfg.force = true;
        }
        return qualbum::run(cfg);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "qualbum: %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "qualbum: unknown error\n");
        return 1;
    }
}
