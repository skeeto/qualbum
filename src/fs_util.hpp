#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace qualbum::fsu {

namespace fs = std::filesystem;

// Like `mkdir -p`.
void mkdir_p(const fs::path& p);

// `route` is a slash-delimited path (e.g. "/space/moon01/thumb.jpg"). Joins
// it onto `destination`, treating the leading slash as a no-op so the result
// stays inside `destination`.
fs::path route_to_path(const fs::path& destination, std::string_view route);

// Remove dst if it exists, then create a hard link from src to dst.
// Falls back to copy_file(overwrite_existing) if hard linking is not
// supported on the destination volume (e.g. cross-volume, FAT32, some CIFS).
void hard_link(const fs::path& src, const fs::path& dst);

// True if `b` does not exist, or mtime(a) > mtime(b). Mirrors qualbum.py:24.
bool newer(const fs::path& a, const fs::path& b);

// Walk the gallery tree rooted at `root`, invoking `callback` for every
// regular file. Skips any directory whose basename begins with '.' or '_'.
// Walk order is the same as Python's os.walk default (alphabetical within a
// directory) so we get deterministic output.
void walk_gallery(const fs::path& root,
                  const std::function<void(const fs::path& relpath)>& callback);

}  // namespace qualbum::fsu
