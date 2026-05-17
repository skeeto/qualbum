#include "photo.hpp"

#include "fs_util.hpp"
#include "image_io.hpp"
#include "image_resize.hpp"
#include "markdown.hpp"
#include "string_util.hpp"

#include <stdexcept>

namespace qualbum {

namespace fs = std::filesystem;

namespace {

std::string make_route(const fs::path& base_rel, std::string_view prefix,
                       bool trailing_slash) {
    std::string out{prefix};
    out.push_back('/');
    bool first = true;
    for (const auto& part : base_rel) {
        if (!first) out.push_back('/');
        out.append(part.generic_string());
        first = false;
    }
    if (trailing_slash) out.push_back('/');
    return out;
}

void apply_root_href_prefix(html::Node& root, std::string_view prefix) {
    if (prefix.empty()) return;
    for (auto* n : html::select_all(root, ".root-href")) {
        auto orig = n->get_attr("href");
        n->set_attr("href", std::string{prefix} + orig);
    }
}

void clone_into(html::Node& target, const html::Node& source_fragment) {
    for (const auto& c : source_fragment.children) {
        auto cc = c->clone_deep();
        target.append(std::move(cc));
    }
}

}  // namespace

Photo load_photo(const fs::path& md_file, const Config& cfg) {
    Photo p;
    p.source_md = md_file;
    auto text = strutil::read_file(md_file.string());
    auto fm = yamlx::parse_frontmatter(text);
    p.meta = std::move(fm.meta);
    p.title = p.meta.get_string("title", "?");
    auto ts = p.meta.try_get_timestamp("date");
    p.date = ts ? *ts : dt::now_local();
    p.gravity = static_cast<int>(p.meta.get_int("gravity", 50));
    p.body_html = md::render(fm.body);

    fs::path base = md_file;
    base.replace_extension();
    auto rel = base.lexically_normal();

    fs::path jpg = base;
    jpg.replace_extension(".jpg");
    p.source_jpg = jpg;

    // Routes are relative to the gallery root and start with '/'.
    fs::path rel_for_route = fs::relative(rel, ".");
    p.route = make_route(rel_for_route, cfg.prefix, true);
    fs::path jpg_rel = fs::relative(jpg.lexically_normal(), ".");
    p.photo_route = make_route(jpg_rel, cfg.prefix, false);
    return p;
}

void emit_photo_page(const Photo& photo,
                     const html::Node& template_doc,
                     const Photo* prev, const Photo* next,
                     const Config& cfg) {
    auto dom = template_doc.clone_deep();

    html::select(*dom, "title").set_text(photo.title);
    html::select(*dom, "#title").set_text(photo.title);

    auto& full_link = html::select(*dom, "#full");
    full_link.set_attr("href", photo.photo_route);
    // The wrapping anchor is the aspect-ratio'd block (see main.css for
    // why). Set `--aspect` per-photo so the CSS picks it up.
    if (photo.preview_w > 0 && photo.preview_h > 0) {
        full_link.set_attr("style",
            "--aspect: " + std::to_string(photo.preview_w)
            + " / " + std::to_string(photo.preview_h));
    }

    auto& img = html::select(*dom, "#photo");
    img.set_attr("src", "preview.jpg");
    if (!photo.preview_bg_hex.empty()) {
        img.set_attr("style", "background-color: " + photo.preview_bg_hex);
    }

    if (prev) {
        if (auto* n = html::select_first(*dom, "#prev")) {
            n->set_attr("href", prev->route);
        }
    }
    if (next) {
        if (auto* n = html::select_first(*dom, "#next")) {
            n->set_attr("href", next->route);
        }
    }

    apply_root_href_prefix(*dom, cfg.prefix);

    auto& time_el = html::select(*dom, "time");
    time_el.set_text(dt::format_display(photo.date));
    time_el.set_attr("datetime", dt::to_iso8601_with_trailing_z(photo.date));

    auto set_meta = [&](std::string_view selector, std::string_view value) {
        if (value.empty()) return;
        if (auto* n = html::select_first(*dom, selector)) {
            n->set_text(std::string{value});
        }
    };
    set_meta("#f-stop",        photo.meta.get_string("f-stop"));
    set_meta("#exposure-time", photo.meta.get_string("exposure-time"));
    set_meta("#iso",           photo.meta.get_string("iso"));

    // Inject parsed markdown body into #info.
    auto& info = html::select(*dom, "#info");
    auto body_doc = html::parse(photo.body_html);
    clone_into(info, *body_doc);

    auto dest_dir = fsu::route_to_path(cfg.destination, photo.route);
    fsu::mkdir_p(dest_dir);
    auto idx = dest_dir / "index.html";
    strutil::write_file_atomic(idx.string(), html::serialize(*dom));
}

namespace {

std::string hex_color(std::array<std::uint8_t, 3> rgb) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string s;
    s.reserve(7);
    s.push_back('#');
    for (auto b : rgb) {
        s.push_back(digits[(b >> 4) & 0xf]);
        s.push_back(digits[b & 0xf]);
    }
    return s;
}

bool resize_needed(const Photo& photo, const Config& cfg,
                   const fs::path& thumb_path, const fs::path& preview_path) {
    if (cfg.force) return true;
    return fsu::newer(photo.source_jpg, thumb_path)
        || fsu::newer(photo.source_jpg, preview_path);
}

}  // namespace

void process_photo_images(Photo& photo, const Config& cfg) {
    auto thumb_path = fsu::route_to_path(cfg.destination, photo.route + "thumb.jpg");
    auto preview_path = fsu::route_to_path(cfg.destination, photo.route + "preview.jpg");

    image::RgbImage preview;
    if (resize_needed(photo, cfg, thumb_path, preview_path)) {
        auto src = image::decode_jpeg(photo.source_jpg);
        auto thumb = image::make_thumbnail(src, photo.gravity, cfg.thumbsize);
        image::encode_jpeg(thumb_path, thumb, 90, true);
        preview = image::make_preview(src, cfg.previewsize);
        image::encode_jpeg(preview_path, preview, 75, true);
    } else {
        // Up-to-date on disk; decode the cached preview to recover the
        // dimensions and pixels we need for the <img> intrinsic size and
        // placeholder background colour.
        preview = image::decode_jpeg(preview_path);
    }

    photo.preview_w = preview.width;
    photo.preview_h = preview.height;
    photo.preview_bg_hex = hex_color(image::compute_average_rgb(preview));
}

}  // namespace qualbum
