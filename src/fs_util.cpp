#include "fs_util.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace qualbum::fsu {

void mkdir_p(const fs::path& p) {
    if (p.empty()) return;
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec && !fs::is_directory(p)) {
        throw std::runtime_error(
            "mkdir failed: " + p.string() + " (" + ec.message() + ")");
    }
}

fs::path route_to_path(const fs::path& destination, std::string_view route) {
    while (!route.empty() && route.front() == '/') route.remove_prefix(1);
    if (route.empty()) return destination;
    return destination / fs::path(std::string{route});
}

void hard_link(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::remove(dst, ec);  // ignore "not found"
    mkdir_p(dst.parent_path());
    fs::create_hard_link(src, dst, ec);
    if (!ec) return;
    // Hard link failed (cross-volume, FAT32, CIFS, Windows mismatch). Copy.
    std::error_code ec2;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec2);
    if (ec2) {
        throw std::runtime_error(
            "link/copy failed: " + dst.string() + " (" + ec2.message() + ")");
    }
}

bool newer(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    if (!fs::exists(b, ec)) return true;
    auto ta = fs::last_write_time(a, ec);
    if (ec) return true;
    auto tb = fs::last_write_time(b, ec);
    if (ec) return true;
    return ta > tb;
}

void walk_gallery(const fs::path& root,
                  const std::function<void(const fs::path&)>& callback) {
    std::vector<fs::path> stack;
    stack.push_back(root);

    while (!stack.empty()) {
        fs::path dir = std::move(stack.back());
        stack.pop_back();

        std::vector<fs::directory_entry> entries;
        std::error_code ec;
        for (auto& ent : fs::directory_iterator(dir, ec)) {
            entries.push_back(ent);
        }
        if (ec) continue;

        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) {
                      return a.path().filename() < b.path().filename();
                  });

        std::vector<fs::path> subdirs;
        for (auto& ent : entries) {
            const auto& name = ent.path().filename().string();
            if (name.empty() || name[0] == '.' || name[0] == '_') {
                // Always skip dot/underscore directories; for files, leave
                // them in so the caller can decide what to do (e.g. read
                // `_gallery.yaml` for galleries). However we also need to
                // surface the bare filename to the caller — actually we
                // expose all entries; underscore-files are returned and
                // filtered by the caller.
            }
            if (ent.is_directory(ec)) {
                if (!name.empty() && (name[0] == '.' || name[0] == '_')) {
                    continue;
                }
                subdirs.push_back(ent.path());
            } else if (ent.is_regular_file(ec)) {
                callback(fs::relative(ent.path(), root, ec));
            }
        }

        // Push reverse so first sub goes on top of the stack and is popped
        // first → matches Python os.walk top-down ordering within siblings.
        for (auto it = subdirs.rbegin(); it != subdirs.rend(); ++it) {
            stack.push_back(*it);
        }
    }
}

}  // namespace qualbum::fsu
