#include "gallery.hpp"

#include "feed.hpp"
#include "fs_util.hpp"
#include "string_util.hpp"
#include "yaml_subset.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace qualbum {

namespace fs = std::filesystem;

namespace {

std::string make_dir_route(const fs::path& rel, std::string_view prefix) {
    std::string out{prefix};
    out.push_back('/');
    bool first = true;
    for (const auto& part : rel) {
        if (!first) out.push_back('/');
        out.append(part.generic_string());
        first = false;
    }
    out.push_back('/');
    return out;
}

void apply_root_href_prefix(html::Node& root, std::string_view prefix) {
    if (prefix.empty()) return;
    for (auto* n : html::select_all(root, ".root-href")) {
        auto orig = n->get_attr("href");
        n->set_attr("href", std::string{prefix} + orig);
    }
}

}  // namespace

Gallery Gallery::load(const fs::path& yaml_file, const Config& cfg) {
    Gallery g;
    auto text = strutil::read_file(yaml_file.string());
    auto m = yamlx::parse_file(text);
    g.path = yaml_file.parent_path().lexically_normal();
    fs::path rel = fs::relative(g.path, ".");
    g.route = make_dir_route(rel, cfg.prefix);
    g.title = m.get_string("title", cfg.title);
    g.cover_image = m.get_string("image");
    return g;
}

Gallery Gallery::root(const Config& cfg) {
    Gallery g;
    g.route = cfg.prefix + "/";
    g.title = cfg.title;
    return g;
}

void Gallery::gather(const Config& cfg) {
    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (auto& e : fs::directory_iterator(path, ec)) entries.push_back(e);
    if (ec) return;
    // No std::sort: matches qualbum.py's os.listdir filesystem-order
    // traversal so the resulting feeds stay comparable to the Python output.
    for (auto& e : entries) {
        if (!e.is_regular_file(ec)) continue;
        auto name = e.path().filename().string();
        if (name.empty() || name[0] == '_' || name[0] == '.') continue;
        if (!strutil::ends_with(name, ".md")) continue;
        photos.push_back(load_photo(e.path(), cfg));
    }
}

namespace {

// Build the <title> head element string (with " » Site" suffix when the
// gallery title differs from the site title). Mirrors qualbum.py:234-237.
std::string make_head_title(const Gallery& g, const Config& cfg, int page_num) {
    std::string tail;
    if (page_num != 1) {
        tail = " (page " + std::to_string(page_num) + ")";
    }
    if (g.title == cfg.title) {
        return g.title + tail;
    }
    return g.title + tail + " \xc2\xbb " + cfg.title;
}

// Build the <h1 id="title"> string. Never includes the " » Site" suffix.
// Mirrors qualbum.py:238.
std::string make_header_title(const Gallery& g, int page_num) {
    std::string tail;
    if (page_num != 1) {
        tail = " (page " + std::to_string(page_num) + ")";
    }
    return g.title + tail;
}

// Append one photo's gallery-page <li> entry to the #gallery <ul>.
void append_photo_li(html::Node& gallery_ul, const Photo& p, const Config& cfg) {
    auto* li = gallery_ul.append_element("li");
    auto* h2 = li->append_element("h2");
    h2->set_text(p.title);
    auto* a = li->append_element("a");
    a->set_attr("href", p.href.empty() ? p.route : p.href);
    auto* img = a->append_element("img");
    img->set_attr("src", p.route + "thumb.jpg");
    img->set_attr("alt", "");
    img->set_attr("title", p.title);
    img->set_attr("width", std::to_string(cfg.thumbsize));
    img->set_attr("height", std::to_string(cfg.thumbsize));
}

// Determine prev/next href for pagination, matching qualbum.py:244-258.
struct PageNav { std::string prev; std::string next; };
PageNav make_page_nav(int page_num, bool last) {
    PageNav nav;
    if (page_num == 1)       nav.prev = "#";
    else if (page_num == 2)  nav.prev = "../";
    else                     nav.prev = "../" + std::to_string(page_num - 1) + "/";
    if (last)                nav.next = "#";
    else if (page_num == 1)  nav.next = "2/";
    else                     nav.next = "../" + std::to_string(page_num + 1) + "/";
    return nav;
}

void emit_one_page(const Gallery& g, std::span<const Photo* const> page_photos,
                   int page_num, bool last,
                   const html::Node& gallery_template, const Config& cfg) {
    auto dom = gallery_template.clone_deep();

    html::select(*dom, "title").set_text(make_head_title(g, cfg, page_num));
    html::select(*dom, "#title").set_text(make_header_title(g, page_num));

    apply_root_href_prefix(*dom, cfg.prefix);

    auto& gallery_ul = html::select(*dom, "#gallery");
    gallery_ul.clear_children();
    for (const Photo* p : page_photos) {
        append_photo_li(gallery_ul, *p, cfg);
    }

    auto nav = make_page_nav(page_num, last);
    html::select(*dom, "#prev").set_attr("href", nav.prev);
    html::select(*dom, "#next").set_attr("href", nav.next);

    // The template emits `href="feed/"` for both the head <link rel=
    // "alternate"> and the body's icon link. On page 1 (route .../) that
    // resolves to .../feed/ — correct. On page N>1 the document lives at
    // .../N/index.html so "feed/" resolves to .../N/feed/, a non-existent
    // per-page feed. Rewrite to ../feed/ so it points back at the gallery's
    // actual feed.
    if (page_num != 1) {
        for (auto* el : html::select_all(*dom, "[href=\"feed/\"]")) {
            el->set_attr("href", "../feed/");
        }
    }

    std::string route = g.route;
    if (page_num != 1) {
        route += std::to_string(page_num) + "/";
    }
    auto dest = fsu::route_to_path(cfg.destination, route);
    fsu::mkdir_p(dest);
    auto idx = dest / "index.html";
    strutil::write_file_atomic(idx.string(), html::serialize(*dom));
}

}  // namespace

void emit_gallery(const Gallery& g,
                  std::vector<const Photo*> photos_in,
                  const html::Node& gallery_template,
                  const html::Node& feed_template,
                  bool emit_feed,
                  const Config& cfg) {
    std::sort(photos_in.begin(), photos_in.end(),
              [](const Photo* a, const Photo* b) {
                  return dt::earlier(b->date, a->date);  // descending
              });

    if (emit_feed) {
        Feed feed;
        feed.route = g.route + "feed/";
        // qualbum.py passes self.title through unchanged. When the gallery
        // is the root (title == site title), Python emits a redundant
        // "Site » Site" string — we reproduce that for parity.
        feed.title = g.title;
        feed.config = &cfg;
        for (const Photo* p : photos_in) feed.append(*p);
        feed.close(feed_template);
    }

    int total = static_cast<int>(photos_in.size());
    int pagemax = std::max(1, cfg.pagemax);
    int pages = (total + pagemax - 1) / pagemax;
    if (pages == 0) pages = 1;
    for (int page = 1; page <= pages; ++page) {
        int start = (page - 1) * pagemax;
        int end = std::min(total, start + pagemax);
        std::span<const Photo* const> slice(photos_in.data() + start,
                                            static_cast<std::size_t>(end - start));
        emit_one_page(g, slice, page, page == pages, gallery_template, cfg);
    }
}

}  // namespace qualbum
