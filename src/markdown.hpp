#pragma once

#include <string>
#include <string_view>

namespace qualbum::md {

// Render a markdown body as the HTML fragment mistune 0.8.4 would produce
// for the inline subset present in this gallery: paragraphs, **bold**/__bold__,
// *italic*/_italic_, `code`, [text](url), autolinks <http(s)://...>, hard line
// breaks (trailing two spaces + newline). Output ends with a trailing newline
// after each paragraph close, matching mistune's behavior.
std::string render(std::string_view body);

}  // namespace qualbum::md
