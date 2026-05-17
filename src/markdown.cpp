#include "markdown.hpp"

#include "string_util.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace qualbum::md {

namespace {

// Mistune-compatible escaper for *text content*: only `& < >` become
// entities. Double quotes are preserved as-is because they need no escaping
// inside element text. Use `escape_attr_append` for attribute values.
void escape_text_append(std::string& out, std::string_view in) {
    for (char c : in) {
        switch (c) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;");  break;
            case '>': out.append("&gt;");  break;
            default:  out.push_back(c);
        }
    }
}

void escape_attr_append(std::string& out, std::string_view in) {
    for (char c : in) {
        switch (c) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;");  break;
            case '>': out.append("&gt;");  break;
            case '"': out.append("&quot;"); break;
            default:  out.push_back(c);
        }
    }
}

// Returns true if `c` is an ASCII punctuation char a backslash may escape.
bool is_md_punct(char c) {
    switch (c) {
        case '\\': case '`':  case '*':  case '_':
        case '{':  case '}':  case '[':  case ']':
        case '(':  case ')':  case '#':  case '+':
        case '-':  case '.':  case '!':  case '|':
        case '>':  case '~':
            return true;
        default:
            return false;
    }
}

bool looks_like_url_scheme(std::string_view sv) {
    auto colon = sv.find(':');
    if (colon == std::string_view::npos) return false;
    if (colon == 0) return false;
    for (std::size_t i = 0; i < colon; ++i) {
        char c = sv[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool looks_like_email(std::string_view sv) {
    auto at = sv.find('@');
    if (at == std::string_view::npos || at == 0 || at + 1 >= sv.size())
        return false;
    for (char c : sv) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '<' || c == '>') return false;
    }
    return true;
}

// Find balanced closing `)` for a link URL, accounting for nested `()` pairs.
std::size_t find_link_close(std::string_view sv, std::size_t start) {
    int depth = 0;
    for (std::size_t i = start; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '\\' && i + 1 < sv.size()) { ++i; continue; }
        if (c == '(') ++depth;
        else if (c == ')') {
            if (depth == 0) return i;
            --depth;
        }
    }
    return std::string_view::npos;
}

void render_inline(std::string_view in, std::string& out) {
    std::size_t i = 0;
    while (i < in.size()) {
        char c = in[i];

        if (c == '\\' && i + 1 < in.size() && is_md_punct(in[i + 1])) {
            escape_text_append(out, std::string_view(&in[i + 1], 1));
            i += 2;
            continue;
        }

        // Hard line break: two or more trailing spaces before '\n'.
        if (c == ' ' && i + 2 < in.size() && in[i + 1] == ' ') {
            std::size_t j = i;
            while (j < in.size() && in[j] == ' ') ++j;
            if (j < in.size() && in[j] == '\n') {
                out.append("<br>\n");
                i = j + 1;
                continue;
            }
        }

        // Code span: pick the shortest closing run of equal length.
        if (c == '`') {
            std::size_t run_start = i;
            while (i < in.size() && in[i] == '`') ++i;
            std::size_t open_len = i - run_start;
            std::size_t scan = i;
            std::size_t close_pos = std::string_view::npos;
            while (scan < in.size()) {
                if (in[scan] == '`') {
                    std::size_t k = scan;
                    while (k < in.size() && in[k] == '`') ++k;
                    if (k - scan == open_len) {
                        close_pos = scan;
                        break;
                    }
                    scan = k;
                } else {
                    ++scan;
                }
            }
            if (close_pos == std::string_view::npos) {
                // Emit literal backticks.
                escape_text_append(out, std::string_view(in.data() + run_start, open_len));
                continue;
            }
            std::string_view inner = in.substr(i, close_pos - i);
            // Trim a single leading and trailing space (CommonMark rule), only
            // when content is non-blank and starts/ends with space.
            if (inner.size() >= 2 && inner.front() == ' ' && inner.back() == ' '
                && inner.find_first_not_of(' ') != std::string_view::npos) {
                inner.remove_prefix(1);
                inner.remove_suffix(1);
            }
            out.append("<code>");
            escape_text_append(out, inner);
            out.append("</code>");
            i = close_pos + open_len;
            continue;
        }

        // Autolink: <scheme://...> or <email@host>.
        if (c == '<') {
            std::size_t end = in.find('>', i + 1);
            if (end != std::string_view::npos) {
                std::string_view inner = in.substr(i + 1, end - i - 1);
                if (looks_like_url_scheme(inner)) {
                    out.append("<a href=\"");
                    escape_attr_append(out, inner);
                    out.append("\">");
                    escape_text_append(out, inner);
                    out.append("</a>");
                    i = end + 1;
                    continue;
                }
                if (looks_like_email(inner)) {
                    out.append("<a href=\"mailto:");
                    escape_attr_append(out, inner);
                    out.append("\">");
                    escape_text_append(out, inner);
                    out.append("</a>");
                    i = end + 1;
                    continue;
                }
            }
            out.append("&lt;");
            ++i;
            continue;
        }

        // Link: [text](url)
        if (c == '[') {
            // Find matching `]` (no nesting of brackets).
            std::size_t close_text = std::string_view::npos;
            for (std::size_t j = i + 1; j < in.size(); ++j) {
                if (in[j] == '\\' && j + 1 < in.size()) { ++j; continue; }
                if (in[j] == ']') { close_text = j; break; }
            }
            if (close_text != std::string_view::npos
                && close_text + 1 < in.size() && in[close_text + 1] == '(') {
                std::size_t url_start = close_text + 2;
                std::size_t url_end = find_link_close(in, url_start);
                if (url_end != std::string_view::npos) {
                    std::string_view text = in.substr(i + 1, close_text - i - 1);
                    std::string_view url = in.substr(url_start, url_end - url_start);
                    out.append("<a href=\"");
                    escape_attr_append(out, url);
                    out.append("\">");
                    render_inline(text, out);
                    out.append("</a>");
                    i = url_end + 1;
                    continue;
                }
            }
            // Not a link; emit '[' as literal.
            out.push_back('[');
            ++i;
            continue;
        }

        // Triple emphasis (***...*** or ___...___) — handled before the
        // strong/em greedy matchers because the trio is ambiguous to a simple
        // greedy scan. Matches mistune 0.8.4 output.
        if ((c == '*' || c == '_') && i + 2 < in.size()
            && in[i + 1] == c && in[i + 2] == c) {
            char d = c;
            std::size_t scan = i + 3;
            std::size_t close = std::string_view::npos;
            while (scan + 2 < in.size()) {
                if (in[scan] == '\\') { scan += 2; continue; }
                if (in[scan] == d && in[scan + 1] == d && in[scan + 2] == d) {
                    close = scan;
                    break;
                }
                ++scan;
            }
            if (close != std::string_view::npos) {
                out.append("<strong><em>");
                render_inline(in.substr(i + 3, close - i - 3), out);
                out.append("</em></strong>");
                i = close + 3;
                continue;
            }
        }

        // Strong (**...** or __...__) — must precede emphasis check.
        if ((c == '*' || c == '_') && i + 1 < in.size() && in[i + 1] == c) {
            char d = c;
            std::size_t scan = i + 2;
            std::size_t close = std::string_view::npos;
            while (scan + 1 < in.size()) {
                if (in[scan] == '\\') { scan += 2; continue; }
                if (in[scan] == d && in[scan + 1] == d) { close = scan; break; }
                ++scan;
            }
            if (close != std::string_view::npos) {
                out.append("<strong>");
                render_inline(in.substr(i + 2, close - i - 2), out);
                out.append("</strong>");
                i = close + 2;
                continue;
            }
        }

        // Emphasis (*...* or _..._)
        if (c == '*' || c == '_') {
            char d = c;
            std::size_t scan = i + 1;
            std::size_t close = std::string_view::npos;
            while (scan < in.size()) {
                if (in[scan] == '\\') { scan += 2; continue; }
                if (in[scan] == d) { close = scan; break; }
                ++scan;
            }
            if (close != std::string_view::npos && close > i + 1) {
                out.append("<em>");
                render_inline(in.substr(i + 1, close - i - 1), out);
                out.append("</em>");
                i = close + 1;
                continue;
            }
        }

        // Default: html-escape and advance one char.
        escape_text_append(out, std::string_view(&in[i], 1));
        ++i;
    }
}

// Strip CR (CRLF normalization), then leading ASCII whitespace, then
// trailing whitespace — except keep two trailing spaces as a hard-break
// marker. Mirrors mistune 0.8.4 / CommonMark line normalization.
std::string_view sanitize_line(std::string_view line) {
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
        line.remove_prefix(1);
    }
    std::size_t end_pos = line.size();
    while (end_pos > 0 && line[end_pos - 1] == ' ') --end_pos;
    std::size_t trailing_spaces = line.size() - end_pos;
    if (trailing_spaces >= 2) {
        return line.substr(0, end_pos + 2);   // keep "  " for hard break
    }
    while (end_pos > 0 && (line[end_pos - 1] == ' ' || line[end_pos - 1] == '\t')) {
        --end_pos;
    }
    return line.substr(0, end_pos);
}

}  // namespace

std::string render(std::string_view body) {
    std::string out;
    std::size_t i = 0;
    auto eat_blank_lines = [&]() {
        while (i < body.size()) {
            std::size_t lineend = body.find('\n', i);
            std::size_t end = (lineend == std::string_view::npos) ? body.size() : lineend;
            auto line = sanitize_line(body.substr(i, end - i));
            if (!line.empty()) break;
            i = (lineend == std::string_view::npos) ? body.size() : lineend + 1;
        }
    };

    eat_blank_lines();

    while (i < body.size()) {
        std::string paragraph;
        bool first_line = true;
        while (i < body.size()) {
            std::size_t lineend = body.find('\n', i);
            std::size_t end = (lineend == std::string_view::npos) ? body.size() : lineend;
            auto line = sanitize_line(body.substr(i, end - i));
            if (line.empty()) break;
            if (!first_line) paragraph.push_back('\n');
            paragraph.append(line);
            first_line = false;
            if (lineend == std::string_view::npos) { i = body.size(); break; }
            i = lineend + 1;
        }
        // Mistune strips trailing whitespace from the paragraph as a whole
        // (so a paragraph never ends with a "hard break" marker).
        while (!paragraph.empty()
               && (paragraph.back() == ' ' || paragraph.back() == '\t'
                || paragraph.back() == '\n')) {
            paragraph.pop_back();
        }
        out.append("<p>");
        render_inline(paragraph, out);
        out.append("</p>\n");
        eat_blank_lines();
    }
    return out;
}

}  // namespace qualbum::md
