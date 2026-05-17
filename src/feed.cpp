#include "feed.hpp"

#include "datetime.hpp"
#include "fs_util.hpp"
#include "html_dom.hpp"
#include "string_util.hpp"
#include "uuid_v3.hpp"

#include <stdexcept>

namespace qualbum {

void Feed::close(const html::Node& feed_template) const {
    if (!config) throw std::runtime_error("Feed::close: no config");
    auto dom = feed_template.clone_deep();

    auto& feed_el = html::select(*dom, "feed");

    auto& title_el = html::select(*dom, "title");
    if (!title.empty()) {
        title_el.set_text(title + " \xc2\xbb " + config->title);
    } else {
        title_el.set_text(config->title);
    }

    if (auto* author = html::select_first(*dom, "author name")) {
        author->set_text(config->author);
    }
    if (auto* updated = html::select_first(*dom, "feed > updated")) {
        updated->set_text(dt::to_iso8601(dt::now_local()));
    }

    // qualbum.py:149 builds the feed identifier from the *gallery* route,
    // not the feed route (which has `feed/` appended). Mirror that so UUID
    // v3 identifiers stay stable across the rewrite.
    std::string gallery_route = route;
    if (gallery_route.ends_with("feed/")) {
        gallery_route.resize(gallery_route.size() - 5);
    }
    std::string feed_url = config->baseurl + config->prefix + gallery_route;
    if (auto* id_el = html::select_first(*dom, "feed > id")) {
        auto u = uuidv3::from_name_str(uuidv3::NAMESPACE_URL, feed_url);
        id_el->set_text("urn:uuid:" + u);
    }
    if (auto* link_self = html::select_first(*dom, "feed link[rel=\"self\"]")) {
        link_self->set_attr("href", config->baseurl + route);
    }

    for (const Photo* photo : photos) {
        auto entry = std::make_unique<html::Node>();
        entry->kind = html::NodeKind::Element;
        entry->tag = "entry";
        entry->xml_mode = true;

        auto& t = *entry->append_element("title");
        t.set_text(photo->title);

        std::string url = config->baseurl
                        + (photo->href.empty() ? photo->route : photo->href);
        auto& id_node = *entry->append_element("id");
        auto u = uuidv3::from_name_str(uuidv3::NAMESPACE_URL, url);
        id_node.set_text("urn:uuid:" + u);

        auto& link = *entry->append_element("link");
        link.set_attr("rel", "alternate");
        link.set_attr("type", "text/html");
        link.set_attr("href", url);

        auto& upd = *entry->append_element("updated");
        upd.set_text(dt::to_iso8601_with_trailing_z(photo->date));

        auto& content = *entry->append_element("content");
        content.set_attr("type", "html");
        // qualbum.py:111 moves photo.dom into the single-page DOM via
        // `append(self.dom)`, which leaves the original soup empty for any
        // feeds emitted afterwards (notably the albums feed). We match that
        // by leaving `<content>` empty when body_html has been cleared by
        // the per-photo emit phase.
        if (!photo->body_html.empty()) {
            content.set_text(photo->body_html);
        }

        feed_el.append(std::move(entry));
    }

    auto dest = fsu::route_to_path(config->destination, route);
    fsu::mkdir_p(dest);
    auto xml_path = dest / "index.xml";
    strutil::write_file_atomic(xml_path.string(), html::serialize(*dom));
}

}  // namespace qualbum
