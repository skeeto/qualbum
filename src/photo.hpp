#pragma once

#include "config.hpp"
#include "datetime.hpp"
#include "html_dom.hpp"
#include "yaml_subset.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace qualbum {

struct Photo {
    std::filesystem::path source_md;
    std::filesystem::path source_jpg;
    yamlx::Map meta;
    std::string title;
    dt::Timestamp date;
    int gravity{50};
    std::string body_html;       // mistune output (HTML fragment)
    std::string route;           // "/space/moon01/" (with prefix)
    std::string photo_route;     // "/space/moon01.jpg" (with prefix)
    std::string href;            // empty unless overridden by album emit

    // Populated by `process_photo_images`. Used by `emit_photo_page` to set
    // intrinsic dimensions on <img id="photo"> (so the browser doesn't
    // reflow when the preview loads) and a placeholder background colour.
    int preview_w{0};
    int preview_h{0};
    std::string preview_bg_hex;  // "#rrggbb" — gamma-correct preview average
};

// Construct a Photo from a `.md` frontmatter+body file.
Photo load_photo(const std::filesystem::path& md_file, const Config& cfg);

// Emit the per-photo `index.html` using the parsed _single.html template.
// `prev` and `next` are nullable.
void emit_photo_page(const Photo& photo,
                     const html::Node& template_doc,
                     const Photo* prev, const Photo* next,
                     const Config& cfg);

// Resize the thumbnail and preview JPEGs when they are stale (or always
// when `cfg.force` is set) and always populates `photo`'s preview metadata
// fields (`preview_w`, `preview_h`, `preview_bg_hex`). The HTML emit phase
// runs after this and consumes those fields.
void process_photo_images(Photo& photo, const Config& cfg);

}  // namespace qualbum
