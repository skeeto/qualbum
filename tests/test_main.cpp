#include "test_main.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

namespace {

bool name_matches(std::string_view name, std::string_view filter) {
    if (filter.empty()) return true;
    return name.find(filter) != std::string_view::npos;
}

}  // namespace

int main(int argc, char** argv) {
    std::string_view filter;
    bool list_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--list" || a == "-l") {
            list_only = true;
        } else if (a == "--filter" && i + 1 < argc) {
            filter = argv[++i];
        } else if (a.starts_with("--filter=")) {
            filter = a.substr(std::strlen("--filter="));
        } else if (a == "--help" || a == "-h") {
            std::printf("usage: %s [--filter SUBSTR] [--list]\n", argv[0]);
            return 0;
        } else {
            filter = a;
        }
    }

    const auto& cases = qbt::Registry::get().cases();
    if (list_only) {
        for (const auto& c : cases) {
            if (name_matches(c.name, filter)) {
                std::printf("%s\n", c.name.c_str());
            }
        }
        return 0;
    }

    int passed = 0;
    int failed = 0;
    int skipped = 0;
    auto t0 = std::chrono::steady_clock::now();

    for (const auto& c : cases) {
        if (!name_matches(c.name, filter)) {
            ++skipped;
            continue;
        }
        qbt::state() = {};
        std::printf("[ RUN      ] %s\n", c.name.c_str());
        std::fflush(stdout);
        try {
            c.fn();
            if (qbt::state().failures == 0) {
                std::printf("[       OK ] %s (%d checks)\n",
                            c.name.c_str(), qbt::state().checks);
                ++passed;
            } else {
                std::printf("[  FAILED  ] %s (%d/%d failed)\n",
                            c.name.c_str(),
                            qbt::state().failures, qbt::state().checks);
                ++failed;
            }
        } catch (const qbt::FailException& e) {
            std::printf("[  FAILED  ] %s\n  %s\n", c.name.c_str(), e.what());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[  FAILED  ] %s (unexpected exception: %s)\n",
                        c.name.c_str(), e.what());
            ++failed;
        } catch (...) {
            std::printf("[  FAILED  ] %s (unknown exception)\n",
                        c.name.c_str());
            ++failed;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::printf("\n=== %d passed, %d failed, %d skipped in %lld ms ===\n",
                passed, failed, skipped, static_cast<long long>(ms));
    return failed == 0 ? 0 : 1;
}
