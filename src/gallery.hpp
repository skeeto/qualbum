#pragma once

#include "config.hpp"
#include "feed.hpp"
#include "html_dom.hpp"
#include "photo.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace qualbum {

struct Gallery {
    std::filesystem::path path;       // e.g. "space" (root has empty path)
    std::string route;                // "/space/" or "/"
    std::string title;
    std::string cover_image;          // optional: matches Photo.title
    std::vector<Photo> photos;        // only owned by sub-galleries; main borrows pointers

    // Load from a directory's _gallery.yaml (or default if yaml_file empty).
    static Gallery load(const std::filesystem::path& yaml_file, const Config& cfg);
    static Gallery root(const Config& cfg);

    // Gather all `.md` Photos that live directly inside `path`.
    void gather(const Config& cfg);
};

// Pagination-aware gallery emit. Writes:
//   <destination>/<route>index.html       (page 1)
//   <destination>/<route>2/index.html     (page 2)
//   ...
//   <destination>/<route>feed/index.xml   (if emit_feed)
// `photos_in` is borrowed (not owned). Sorted by date desc internally.
void emit_gallery(const Gallery& g,
                  std::vector<const Photo*> photos_in,
                  const html::Node& gallery_template,
                  const html::Node& feed_template,
                  bool emit_feed,
                  const Config& cfg);

}  // namespace qualbum
