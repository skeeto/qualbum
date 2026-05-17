#include "config.hpp"

#include "string_util.hpp"
#include "yaml_subset.hpp"

#include <filesystem>

namespace qualbum {

Config Config::load(const std::filesystem::path& yaml_file) {
    Config cfg = defaults();
    std::error_code ec;
    if (!std::filesystem::exists(yaml_file, ec)) return cfg;

    auto text = strutil::read_file(yaml_file.string());
    auto m = yamlx::parse_file(text);

    if (m.contains("title"))       cfg.title = m.get_string("title");
    if (m.contains("author"))      cfg.author = m.get_string("author");
    if (m.contains("baseurl"))     cfg.baseurl = m.get_string("baseurl");
    if (m.contains("prefix"))      cfg.prefix = m.get_string("prefix");
    if (m.contains("destination")) cfg.destination = m.get_string("destination");
    if (m.contains("thumbsize"))   cfg.thumbsize = static_cast<int>(m.get_int("thumbsize", 300));
    // qualbum.py:46 read the wrong key here; we read the right one.
    if (m.contains("previewsize")) cfg.previewsize = static_cast<int>(m.get_int("previewsize", 1200));
    if (m.contains("pagemax"))     cfg.pagemax = static_cast<int>(m.get_int("pagemax", 48));
    if (m.contains("force"))       cfg.force = (m.get_string("force") == "true");

    return cfg;
}

}  // namespace qualbum
