#pragma once

#include <string>
#include <string_view>

namespace qualbum::strutil {

// HTML-escape: & < > and (when in_attr) " all become entities.
// Newlines/tabs preserved verbatim.
void html_escape_append(std::string& out, std::string_view in, bool in_attr);
std::string html_escape(std::string_view in, bool in_attr = false);

// Append/return XML 1.0 escaped text (& < > " '), suitable for both element
// content and attribute values.
void xml_escape_append(std::string& out, std::string_view in);
std::string xml_escape(std::string_view in);

// Trim leading and trailing ASCII whitespace (space, tab, CR, LF).
std::string_view rstrip(std::string_view sv);
std::string_view lstrip(std::string_view sv);
std::string_view strip(std::string_view sv);

bool starts_with(std::string_view sv, std::string_view prefix);
bool ends_with(std::string_view sv, std::string_view suffix);

std::string lower_ascii(std::string_view sv);

// Read entire file as UTF-8. Throws std::runtime_error on I/O failure.
std::string read_file(std::string_view path);

// Atomic write: write to path+".tmp" then rename. Throws on failure.
void write_file_atomic(std::string_view path, std::string_view contents);

}  // namespace qualbum::strutil
