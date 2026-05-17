#pragma once

#include "config.hpp"
#include "html_dom.hpp"
#include "photo.hpp"

#include <string>
#include <vector>

namespace qualbum {

struct Feed {
    std::string route;     // ends in "feed/"
    std::string title;     // gallery title or empty for site root
    const Config* config;
    std::vector<const Photo*> photos;

    void append(const Photo& p) { photos.push_back(&p); }
    void close(const html::Node& feed_template) const;
};

}  // namespace qualbum
