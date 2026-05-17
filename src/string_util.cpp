#include "string_util.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace qualbum::strutil {

void html_escape_append(std::string& out, std::string_view in, bool in_attr) {
    out.reserve(out.size() + in.size());
    for (char c : in) {
        switch (c) {
            case '&':  out.append("&amp;");  break;
            case '<':  out.append("&lt;");   break;
            case '>':  out.append("&gt;");   break;
            case '"':
                if (in_attr) out.append("&quot;");
                else         out.push_back(c);
                break;
            default:
                out.push_back(c);
        }
    }
}

std::string html_escape(std::string_view in, bool in_attr) {
    std::string s;
    html_escape_append(s, in, in_attr);
    return s;
}

void xml_escape_append(std::string& out, std::string_view in) {
    out.reserve(out.size() + in.size());
    for (char c : in) {
        switch (c) {
            case '&':  out.append("&amp;");  break;
            case '<':  out.append("&lt;");   break;
            case '>':  out.append("&gt;");   break;
            case '"':  out.append("&quot;"); break;
            case '\'': out.append("&apos;"); break;
            default:   out.push_back(c);
        }
    }
}

std::string xml_escape(std::string_view in) {
    std::string s;
    xml_escape_append(s, in);
    return s;
}

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string_view rstrip(std::string_view sv) {
    while (!sv.empty() && is_ws(sv.back())) sv.remove_suffix(1);
    return sv;
}

std::string_view lstrip(std::string_view sv) {
    while (!sv.empty() && is_ws(sv.front())) sv.remove_prefix(1);
    return sv;
}

std::string_view strip(std::string_view sv) {
    return rstrip(lstrip(sv));
}

bool starts_with(std::string_view sv, std::string_view prefix) {
    return sv.size() >= prefix.size()
        && sv.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(std::string_view sv, std::string_view suffix) {
    return sv.size() >= suffix.size()
        && sv.compare(sv.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lower_ascii(std::string_view sv) {
    std::string out;
    out.resize(sv.size());
    for (std::size_t i = 0; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
        out[i] = static_cast<char>(c);
    }
    return out;
}

std::string read_file(std::string_view path) {
    std::filesystem::path p{std::string(path)};
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot open file: " + std::string(path));
    }
    std::string out;
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    if (sz > 0) {
        out.resize(static_cast<std::size_t>(sz));
        f.seekg(0, std::ios::beg);
        f.read(out.data(), sz);
        if (!f) {
            throw std::runtime_error("read failed: " + std::string(path));
        }
    }
    return out;
}

void write_file_atomic(std::string_view path, std::string_view contents) {
    std::filesystem::path final_path{std::string(path)};
    std::filesystem::path tmp_path = final_path;
    tmp_path += ".tmp";
    {
        std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
        if (!f) {
            throw std::runtime_error("cannot open for write: " + std::string(path));
        }
        if (!contents.empty()) {
            f.write(contents.data(),
                    static_cast<std::streamsize>(contents.size()));
        }
        if (!f) {
            throw std::runtime_error("write failed: " + std::string(path));
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp_path, final_path, ec);
    if (ec) {
        // Windows: rename fails if target exists. Remove and retry.
        std::filesystem::remove(final_path, ec);
        std::filesystem::rename(tmp_path, final_path, ec);
        if (ec) {
            throw std::runtime_error(
                "rename failed: " + std::string(path) + " (" + ec.message() + ")");
        }
    }
}

}  // namespace qualbum::strutil
