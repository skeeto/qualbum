#include "app.hpp"

#include "fs_util.hpp"
#include "gallery.hpp"
#include "html_dom.hpp"
#include "photo.hpp"
#include "string_util.hpp"
#include "thread_pool.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <vector>

namespace qualbum {

namespace fs = std::filesystem;

namespace {

bool dir_is_excluded(const std::string& name) {
    return name.empty() || name[0] == '.' || name[0] == '_';
}

void walk_collect(const fs::path& root,
                  std::deque<Gallery>& galleries,
                  std::vector<fs::path>& static_files,
                  const Config& cfg) {
    struct Frame {
        fs::path dir;
    };
    std::vector<Frame> stack{{root}};
    while (!stack.empty()) {
        auto cur = std::move(stack.back());
        stack.pop_back();

        std::error_code ec;
        std::vector<fs::directory_entry> entries;
        for (auto& e : fs::directory_iterator(cur.dir, ec)) {
            entries.push_back(e);
        }
        if (ec) continue;
        // No std::sort: qualbum.py walks filesystem order (os.walk), and
        // we want byte-identical output against the existing _site/.

        bool gallery_here = false;
        std::vector<fs::path> subdirs;
        for (auto& e : entries) {
            auto name = e.path().filename().string();
            if (e.is_directory(ec)) {
                if (dir_is_excluded(name)) continue;
                subdirs.push_back(e.path());
            } else if (e.is_regular_file(ec)) {
                if (name == "_gallery.yaml") {
                    gallery_here = true;
                } else if (!name.empty() && name[0] != '_') {
                    static_files.push_back(e.path());
                }
            }
        }

        if (gallery_here) {
            Gallery g = Gallery::load(cur.dir / "_gallery.yaml", cfg);
            g.gather(cfg);
            galleries.push_back(std::move(g));
        }

        for (auto it = subdirs.rbegin(); it != subdirs.rend(); ++it) {
            stack.push_back({*it});
        }
    }
}

}  // namespace

int run(const Config& cfg) {
    // Templates loaded once and cloned per page.
    auto gallery_tpl_src = strutil::read_file("_gallery.html");
    auto single_tpl_src  = strutil::read_file("_single.html");
    auto feed_tpl_src    = strutil::read_file("_feed.xml");
    auto gallery_tpl     = html::parse(gallery_tpl_src);
    auto single_tpl      = html::parse(single_tpl_src);
    auto feed_tpl        = html::parse(feed_tpl_src, html::ParseOptions{true});

    // Phase 1: walk filesystem.
    std::deque<Gallery> galleries;
    std::vector<fs::path> static_files;
    walk_collect(".", galleries, static_files, cfg);

    // Phase 2: hard-link static files into the destination tree.
    for (const auto& src : static_files) {
        auto rel = fs::relative(src, ".");
        auto dest = cfg.destination / rel;
        fsu::hard_link(src, dest);
    }

    // Phase 3: emit each sub-gallery (with feed). After emit, also sort the
    // gallery's photos by date descending — qualbum.py:278 mutates
    // gallery.photos to this order, and the later albums-cover selection
    // (Phase 7) relies on photos.back() being the *oldest* photo.
    std::vector<const Photo*> main_photos;
    for (auto& g : galleries) {
        std::vector<const Photo*> ptrs;
        ptrs.reserve(g.photos.size());
        for (auto& p : g.photos) {
            ptrs.push_back(&p);
            main_photos.push_back(&p);
        }
        emit_gallery(g, ptrs, *gallery_tpl, *feed_tpl, true, cfg);
        std::sort(g.photos.begin(), g.photos.end(),
                  [](const Photo& a, const Photo& b) {
                      return dt::earlier(b.date, a.date);
                  });
    }

    // Phase 4: emit the root gallery (everything sorted by date desc).
    {
        Gallery main_gal = Gallery::root(cfg);
        emit_gallery(main_gal, main_photos, *gallery_tpl, *feed_tpl, true, cfg);
    }

    // Photos in date-desc order — used for both image processing and the
    // serial single-photo HTML emit (where prev/next are stitched).
    std::vector<const Photo*> ordered = main_photos;
    std::sort(ordered.begin(), ordered.end(),
              [](const Photo* a, const Photo* b) {
                  return dt::earlier(b->date, a->date);
              });
    int n = static_cast<int>(ordered.size());

    // Phase 5: parallel image processing. Each job decodes the source (or
    // the cached preview), regenerates thumb+preview when stale, and stores
    // preview dimensions + a placeholder background colour back on the
    // Photo. Single-photo HTML emit (Phase 6) depends on those fields.
    {
        tp::ThreadPool pool(cfg.jobs);
        std::fprintf(stderr,
                     "processing %d image(s) with %u worker(s)...\n",
                     n, static_cast<unsigned>(pool.worker_count()));
        for (const Photo* p : ordered) {
            Photo* pp = const_cast<Photo*>(p);
            const Config* cp = &cfg;
            pool.submit([pp, cp]() {
                try {
                    process_photo_images(*pp, *cp);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, "image processing failed for %s: %s\n",
                                 pp->source_jpg.string().c_str(), e.what());
                }
            });
        }
        pool.wait_idle();
    }

    // Phase 6: serial single-photo HTML emit. Uses preview_w/preview_h/
    // preview_bg_hex populated in Phase 5 to set intrinsic dimensions and
    // a placeholder background on <img id="photo">, so the page no longer
    // reflows when the preview JPEG arrives.
    for (int i = 0; i < n; ++i) {
        const Photo* prev = (i > 0) ? ordered[static_cast<std::size_t>(i - 1)] : nullptr;
        const Photo* next = (i + 1 < n) ? ordered[static_cast<std::size_t>(i + 1)] : nullptr;
        const Photo* cur = ordered[static_cast<std::size_t>(i)];
        emit_photo_page(*cur, *single_tpl, prev, next, cfg);
        // qualbum.py:111 moves photo.dom into the single-page DOM, leaving
        // the original empty. Mirror this so the albums feed (Phase 7) ends
        // up with empty `<content>` elements just like Python's output.
        const_cast<Photo*>(cur)->body_html.clear();
    }

    // Phase 7: albums gallery (cover photo per sub-gallery; mutates titles +
    // hrefs, but those photos won't be re-emitted afterwards).
    Gallery albums;
    albums.title = "List of Albums";
    albums.route = cfg.prefix + "/albums/";
    std::vector<const Photo*> album_photos;
    for (auto& g : galleries) {
        if (g.photos.empty()) continue;
        Photo* cover = &g.photos.back();
        if (!g.cover_image.empty()) {
            // qualbum.py:376-378 iterates without `break`: the last
            // matching photo wins. Combined with g.photos being sorted
            // date-desc, this means we pick the *oldest* matching photo.
            for (auto& cand : g.photos) {
                if (cand.title == g.cover_image) {
                    cover = &cand;
                }
            }
        }
        cover->title = g.title;
        cover->href = g.route;
        album_photos.push_back(cover);
    }
    emit_gallery(albums, album_photos, *gallery_tpl, *feed_tpl, true, cfg);

    std::fprintf(stderr, "qualbum: emitted %zu galleries, %d photos.\n",
                 galleries.size(), n);
    return 0;
}

}  // namespace qualbum
