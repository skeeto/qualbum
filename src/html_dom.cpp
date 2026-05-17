#include "html_dom.hpp"

#include "string_util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

namespace qualbum::html {

// ---------------- Node ---------------------------------------------------

const std::string* Node::attr(std::string_view name) const {
    for (auto& a : attrs) if (a.name == name) return &a.value;
    return nullptr;
}

std::string Node::get_attr(std::string_view name, std::string_view def) const {
    if (auto* p = attr(name)) return *p;
    return std::string{def};
}

void Node::set_attr(std::string_view name, std::string value) {
    for (auto& a : attrs) {
        if (a.name == name) { a.value = std::move(value); return; }
    }
    attrs.push_back({std::string{name}, std::move(value)});
}

bool Node::has_attr(std::string_view name) const {
    return attr(name) != nullptr;
}

bool Node::has_class(std::string_view cls) const {
    const auto* c = attr("class");
    if (!c) return false;
    std::string_view sv = *c;
    while (!sv.empty()) {
        std::size_t s = 0;
        while (s < sv.size() && (sv[s] == ' ' || sv[s] == '\t' || sv[s] == '\n')) ++s;
        std::size_t e = s;
        while (e < sv.size() && !(sv[e] == ' ' || sv[e] == '\t' || sv[e] == '\n')) ++e;
        if (e > s && sv.substr(s, e - s) == cls) return true;
        if (e >= sv.size()) break;
        sv.remove_prefix(e + 1);
    }
    return false;
}

void Node::clear_children() {
    children.clear();
}

Node* Node::append(std::unique_ptr<Node> child) {
    child->parent = this;
    child->xml_mode = xml_mode;
    Node* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

Node* Node::append_element(std::string_view t) {
    auto n = std::make_unique<Node>();
    n->kind = NodeKind::Element;
    n->tag = std::string{t};
    return append(std::move(n));
}

Node* Node::append_text(std::string content) {
    auto n = std::make_unique<Node>();
    n->kind = NodeKind::Text;
    n->text = std::move(content);
    return append(std::move(n));
}

void Node::set_text(std::string content) {
    children.clear();
    append_text(std::move(content));
}

std::unique_ptr<Node> Node::clone_deep() const {
    auto out = std::make_unique<Node>();
    out->kind = kind;
    out->tag = tag;
    out->text = text;
    out->attrs = attrs;
    out->xml_mode = xml_mode;
    out->self_closing = self_closing;
    for (auto& c : children) {
        auto cc = c->clone_deep();
        cc->parent = out.get();
        out->children.push_back(std::move(cc));
    }
    return out;
}

// ---------------- Parser -------------------------------------------------

namespace {

const std::unordered_set<std::string> VOID_ELEMENTS = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr",
};

const std::unordered_set<std::string> RAW_TEXT_ELEMENTS = {
    "script", "style",
};

bool is_void_element(std::string_view tag) {
    return VOID_ELEMENTS.count(std::string{tag}) > 0;
}

bool is_raw_text_element(std::string_view tag) {
    return RAW_TEXT_ELEMENTS.count(std::string{tag}) > 0;
}

bool is_name_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c))
        || c == '-' || c == '_' || c == ':' || c == '.';
}

bool is_html_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

class Parser {
public:
    Parser(std::string_view src, bool xml) : src_(src), pos_(0), xml_(xml) {}

    std::unique_ptr<Node> parse_document() {
        auto doc = std::make_unique<Node>();
        doc->kind = NodeKind::Document;
        doc->xml_mode = xml_;
        parse_into(*doc);
        return doc;
    }

private:
    std::string_view src_;
    std::size_t pos_;
    bool xml_;

    bool eof() const { return pos_ >= src_.size(); }
    char peek(std::size_t off = 0) const {
        return (pos_ + off < src_.size()) ? src_[pos_ + off] : '\0';
    }
    bool starts_with(std::string_view s) const {
        if (pos_ + s.size() > src_.size()) return false;
        return std::memcmp(src_.data() + pos_, s.data(), s.size()) == 0;
    }
    bool starts_with_ci(std::string_view s) const {
        if (pos_ + s.size() > src_.size()) return false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            char a = src_[pos_ + i];
            char b = s[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) return false;
        }
        return true;
    }
    void advance(std::size_t n = 1) {
        pos_ = std::min(pos_ + n, src_.size());
    }

    void skip_ws() {
        while (!eof() && is_html_ws(src_[pos_])) ++pos_;
    }

    std::string read_name() {
        std::string s;
        while (!eof() && is_name_char(src_[pos_])) {
            s.push_back(src_[pos_++]);
        }
        return s;
    }

    // Decode &amp; &lt; &gt; &quot; &apos; &#NN; &#xHH; ; pass other entities
    // through unchanged.
    static std::string decode_entities(std::string_view in) {
        std::string out;
        out.reserve(in.size());
        std::size_t i = 0;
        while (i < in.size()) {
            if (in[i] != '&') { out.push_back(in[i++]); continue; }
            std::size_t semi = in.find(';', i + 1);
            if (semi == std::string_view::npos || semi - i > 16) {
                out.push_back('&');
                ++i;
                continue;
            }
            std::string_view name = in.substr(i + 1, semi - i - 1);
            if (name == "amp")       out.push_back('&');
            else if (name == "lt")   out.push_back('<');
            else if (name == "gt")   out.push_back('>');
            else if (name == "quot") out.push_back('"');
            else if (name == "apos") out.push_back('\'');
            else if (name == "nbsp") {
                out.push_back('\xc2'); out.push_back('\xa0');
            } else if (name.size() > 1 && name[0] == '#') {
                unsigned code = 0;
                bool ok = true;
                if (name[1] == 'x' || name[1] == 'X') {
                    for (std::size_t k = 2; k < name.size(); ++k) {
                        char c = name[k];
                        unsigned d;
                        if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
                        else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
                        else { ok = false; break; }
                        code = code * 16 + d;
                    }
                } else {
                    for (std::size_t k = 1; k < name.size(); ++k) {
                        char c = name[k];
                        if (c < '0' || c > '9') { ok = false; break; }
                        code = code * 10 + static_cast<unsigned>(c - '0');
                    }
                }
                if (ok) {
                    // Emit as UTF-8.
                    if (code < 0x80) {
                        out.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        out.push_back(static_cast<char>(0xc0 | (code >> 6)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    } else if (code < 0x10000) {
                        out.push_back(static_cast<char>(0xe0 | (code >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    } else {
                        out.push_back(static_cast<char>(0xf0 | (code >> 18)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
                    }
                } else {
                    out.append(in.substr(i, semi - i + 1));
                }
            } else {
                out.append(in.substr(i, semi - i + 1));
            }
            i = semi + 1;
        }
        return out;
    }

    void emit_text_until(Node& parent, std::size_t end_excl) {
        if (end_excl <= pos_) return;
        std::string raw{src_.substr(pos_, end_excl - pos_)};
        pos_ = end_excl;
        parent.append_text(decode_entities(raw));
    }

    void parse_comment(Node& parent) {
        // pos_ at '<', source starts with '<!--'
        advance(4);
        auto end = src_.find("-->", pos_);
        std::size_t content_end = (end == std::string_view::npos) ? src_.size() : end;
        auto comment = std::make_unique<Node>();
        comment->kind = NodeKind::Comment;
        comment->text = std::string{src_.substr(pos_, content_end - pos_)};
        parent.append(std::move(comment));
        pos_ = (end == std::string_view::npos) ? src_.size() : end + 3;
    }

    void parse_doctype(Node& parent) {
        // pos_ at '<', source starts with '<!DOCTYPE' (case-insensitive)
        std::size_t end = src_.find('>', pos_);
        std::size_t stop = (end == std::string_view::npos) ? src_.size() : end + 1;
        auto dt = std::make_unique<Node>();
        dt->kind = NodeKind::Doctype;
        dt->text = std::string{src_.substr(pos_, stop - pos_)};
        parent.append(std::move(dt));
        pos_ = stop;
    }

    void parse_pi(Node& parent) {
        // pos_ at '<', starts with '<?'
        std::size_t end = src_.find("?>", pos_);
        std::size_t stop = (end == std::string_view::npos) ? src_.size() : end + 2;
        auto pi = std::make_unique<Node>();
        pi->kind = NodeKind::ProcessingInstruction;
        pi->text = std::string{src_.substr(pos_, stop - pos_)};
        parent.append(std::move(pi));
        pos_ = stop;
    }

    void parse_cdata(Node& parent) {
        // pos_ at '<', starts with '<![CDATA['
        advance(9);
        std::size_t end = src_.find("]]>", pos_);
        std::size_t content_end = (end == std::string_view::npos) ? src_.size() : end;
        auto cd = std::make_unique<Node>();
        cd->kind = NodeKind::CData;
        cd->text = std::string{src_.substr(pos_, content_end - pos_)};
        parent.append(std::move(cd));
        pos_ = (end == std::string_view::npos) ? src_.size() : end + 3;
    }

    // Returns a parsed attribute (consumes name [= value]).
    bool parse_attribute(Attr& out) {
        skip_ws();
        if (eof()) return false;
        char c = peek();
        if (c == '>' || c == '/' || (xml_ && c == '?')) return false;
        std::string name;
        while (!eof() && !is_html_ws(peek()) && peek() != '=' && peek() != '>'
               && peek() != '/' && peek() != '<') {
            name.push_back(src_[pos_++]);
        }
        if (name.empty()) {
            // Skip stray char to avoid infinite loop.
            if (!eof()) ++pos_;
            return false;
        }
        if (!xml_) {
            for (char& ch : name) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            }
        }
        std::string value;
        skip_ws();
        if (peek() == '=') {
            advance();
            skip_ws();
            char q = peek();
            if (q == '"' || q == '\'') {
                advance();
                std::size_t s = pos_;
                while (!eof() && src_[pos_] != q) ++pos_;
                value = decode_entities(src_.substr(s, pos_ - s));
                if (!eof()) advance();  // skip closing quote
            } else {
                std::size_t s = pos_;
                while (!eof() && !is_html_ws(peek()) && peek() != '>') ++pos_;
                value = decode_entities(src_.substr(s, pos_ - s));
            }
        } else {
            // Valueless attribute (e.g. <input disabled>). Value defaults to
            // attribute name in HTML5.
            value = name;
        }
        out.name = std::move(name);
        out.value = std::move(value);
        return true;
    }

    // pos_ at '<', parses '<tag ...' open tag (and may self-close).
    // Returns the new element. Caller decides whether to push it on the stack.
    std::unique_ptr<Node> parse_open_tag(bool& self_closing) {
        advance();  // consume '<'
        std::string tag = read_name();
        if (!xml_) {
            for (char& ch : tag) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            }
        }
        auto el = std::make_unique<Node>();
        el->kind = NodeKind::Element;
        el->tag = std::move(tag);
        el->xml_mode = xml_;

        Attr attr;
        while (parse_attribute(attr)) {
            el->attrs.push_back(std::move(attr));
            attr = {};
        }
        skip_ws();
        self_closing = false;
        if (peek() == '/') {
            self_closing = true;
            advance();
            skip_ws();
        }
        if (peek() == '>') advance();
        return el;
    }

    // Parse a closing tag like '</foo>'. Returns the tag name.
    std::string parse_close_tag() {
        advance(2);  // '</'
        std::string tag = read_name();
        if (!xml_) {
            for (char& ch : tag) {
                if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            }
        }
        while (!eof() && peek() != '>') ++pos_;
        if (!eof()) advance();  // consume '>'
        return tag;
    }

    // Greedy raw-text scan until matching close tag. Used for <script>/<style>.
    void parse_raw_text_until_close(Node& parent, const std::string& tag) {
        std::size_t start = pos_;
        std::string needle = "</" + tag;
        auto end = src_.find(needle, pos_);
        // We must match case-insensitively in HTML. Re-scan if needed.
        if (!xml_ && end == std::string_view::npos) {
            std::string lower = strutil::lower_ascii(src_);
            end = lower.find(needle, pos_);
        }
        std::size_t text_end = (end == std::string_view::npos) ? src_.size() : end;
        std::string raw{src_.substr(start, text_end - start)};
        if (!raw.empty()) parent.append_text(std::move(raw));
        pos_ = text_end;
        if (end != std::string_view::npos) {
            // Consume "</tag.....>"
            while (!eof() && peek() != '>') ++pos_;
            if (!eof()) advance();
        }
    }

    void parse_into(Node& root) {
        std::vector<Node*> stack{&root};
        while (!eof()) {
            if (peek() != '<') {
                std::size_t end = src_.find('<', pos_);
                if (end == std::string_view::npos) end = src_.size();
                emit_text_until(*stack.back(), end);
                continue;
            }
            // We have '<'.
            if (starts_with("<!--")) {
                parse_comment(*stack.back());
            } else if (starts_with_ci("<!doctype")) {
                parse_doctype(*stack.back());
            } else if (xml_ && starts_with("<![CDATA[")) {
                parse_cdata(*stack.back());
            } else if (starts_with("<?")) {
                parse_pi(*stack.back());
            } else if (starts_with("</")) {
                std::string close = parse_close_tag();
                // Pop stack until matching open. If no match, ignore (lenient).
                for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                    if ((*it)->kind == NodeKind::Element && (*it)->tag == close) {
                        // Pop everything down to and including this.
                        std::size_t target = static_cast<std::size_t>(
                            std::distance(stack.begin(), it.base()) - 1);
                        stack.resize(target);
                        goto next_token;
                    }
                }
                next_token: ;
            } else if (peek(1) == '!') {
                // Unknown <!... declaration: skip to '>'.
                std::size_t end = src_.find('>', pos_);
                pos_ = (end == std::string_view::npos) ? src_.size() : end + 1;
            } else {
                bool self_close = false;
                auto el = parse_open_tag(self_close);
                std::string tag = el->tag;
                Node* raw_ptr = stack.back()->append(std::move(el));
                if (!self_close) {
                    if (!xml_ && is_void_element(tag)) {
                        // void: stays leaf in HTML mode
                    } else if (!xml_ && is_raw_text_element(tag)) {
                        parse_raw_text_until_close(*raw_ptr, tag);
                    } else {
                        stack.push_back(raw_ptr);
                    }
                } else if (xml_) {
                    raw_ptr->self_closing = true;
                }
            }
        }
    }
};

// ---------------- Serializer ---------------------------------------------

bool element_is_empty(const Node& el) {
    return el.children.empty();
}

void serialize_text_html(std::string& out, std::string_view text) {
    for (char c : text) {
        switch (c) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            default:  out.push_back(c);
        }
    }
}

void serialize_attr_html(std::string& out, std::string_view text) {
    for (char c : text) {
        switch (c) {
            case '&': out.append("&amp;");  break;
            case '<': out.append("&lt;");   break;
            case '>': out.append("&gt;");   break;
            case '"': out.append("&quot;"); break;
            default:  out.push_back(c);
        }
    }
}

void serialize_text_xml(std::string& out, std::string_view text) {
    for (char c : text) {
        switch (c) {
            case '&': out.append("&amp;");  break;
            case '<': out.append("&lt;");   break;
            case '>': out.append("&gt;");   break;
            default:  out.push_back(c);
        }
    }
}

void serialize_attr_xml(std::string& out, std::string_view text) {
    for (char c : text) {
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

void serialize_node(const Node& n, std::string& out, bool xml, bool raw_text);

void serialize_element(const Node& el, std::string& out, bool xml) {
    out.push_back('<');
    out.append(el.tag);
    for (const auto& a : el.attrs) {
        out.push_back(' ');
        out.append(a.name);
        out.push_back('=');
        out.push_back('"');
        if (xml) serialize_attr_xml(out, a.value);
        else     serialize_attr_html(out, a.value);
        out.push_back('"');
    }
    bool raw_text = !xml && is_raw_text_element(el.tag);
    bool empty = element_is_empty(el);
    if (!xml && is_void_element(el.tag)) {
        out.push_back('>');
        return;
    }
    if (xml && empty) {
        out.append("/>");
        return;
    }
    out.push_back('>');
    for (const auto& c : el.children) {
        serialize_node(*c, out, xml, raw_text);
    }
    out.append("</");
    out.append(el.tag);
    out.push_back('>');
}

void serialize_node(const Node& n, std::string& out, bool xml, bool raw_text) {
    switch (n.kind) {
        case NodeKind::Document:
            for (const auto& c : n.children) {
                serialize_node(*c, out, xml, false);
            }
            break;
        case NodeKind::Element:
            serialize_element(n, out, xml);
            break;
        case NodeKind::Text:
            if (raw_text) {
                out.append(n.text);
            } else if (xml) {
                serialize_text_xml(out, n.text);
            } else {
                serialize_text_html(out, n.text);
            }
            break;
        case NodeKind::Comment:
            out.append("<!--");
            out.append(n.text);
            out.append("-->");
            break;
        case NodeKind::Doctype:
            out.append(n.text);
            break;
        case NodeKind::ProcessingInstruction:
            out.append(n.text);
            break;
        case NodeKind::CData:
            out.append("<![CDATA[");
            out.append(n.text);
            out.append("]]>");
            break;
    }
}

}  // namespace

std::unique_ptr<Node> parse(std::string_view source, ParseOptions opts) {
    Parser p(source, opts.xml);
    return p.parse_document();
}

void serialize(const Node& root, std::string& out) {
    serialize_node(root, out, root.xml_mode, false);
}

std::string serialize(const Node& root) {
    std::string out;
    serialize(root, out);
    return out;
}

// ---------------- Selector ----------------------------------------------

bool SimpleSelector::matches(const Node& n) const {
    if (n.kind != NodeKind::Element) return false;
    if (tag) {
        if (n.tag != *tag) return false;
    }
    if (id) {
        auto* a = n.attr("id");
        if (!a || *a != *id) return false;
    }
    for (const auto& cls : classes) {
        if (!n.has_class(cls)) return false;
    }
    for (const auto& [k, v] : attr_eq) {
        auto* a = n.attr(k);
        if (!a || *a != v) return false;
    }
    return true;
}

namespace {

void walk_descendants(Node& root, const SimpleSelector& s,
                      std::vector<Node*>& out) {
    for (auto& c : root.children) {
        if (s.matches(*c)) out.push_back(c.get());
        walk_descendants(*c, s, out);
    }
}

SimpleSelector parse_simple(std::string_view& sv) {
    SimpleSelector ss;
    bool any = false;
    while (!sv.empty()) {
        char c = sv.front();
        if (c == ' ' || c == '\t' || c == '\n' || c == '>') break;
        if (c == '#') {
            sv.remove_prefix(1);
            std::string id;
            while (!sv.empty() && !std::strchr(" \t\n>.#[", sv.front())) {
                id.push_back(sv.front());
                sv.remove_prefix(1);
            }
            ss.id = std::move(id);
            any = true;
        } else if (c == '.') {
            sv.remove_prefix(1);
            std::string cls;
            while (!sv.empty() && !std::strchr(" \t\n>.#[", sv.front())) {
                cls.push_back(sv.front());
                sv.remove_prefix(1);
            }
            ss.classes.push_back(std::move(cls));
            any = true;
        } else if (c == '[') {
            sv.remove_prefix(1);
            std::string name;
            while (!sv.empty() && sv.front() != '=' && sv.front() != ']') {
                name.push_back(sv.front());
                sv.remove_prefix(1);
            }
            std::string value;
            if (!sv.empty() && sv.front() == '=') {
                sv.remove_prefix(1);
                char q = (!sv.empty() && (sv.front() == '"' || sv.front() == '\''))
                    ? sv.front() : '\0';
                if (q) sv.remove_prefix(1);
                while (!sv.empty() && sv.front() != ']'
                       && (q == '\0' || sv.front() != q)) {
                    value.push_back(sv.front());
                    sv.remove_prefix(1);
                }
                if (q && !sv.empty() && sv.front() == q) sv.remove_prefix(1);
            }
            if (!sv.empty() && sv.front() == ']') sv.remove_prefix(1);
            ss.attr_eq.emplace_back(std::move(name), std::move(value));
            any = true;
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '*'
                   || c == '_') {
            std::string tag;
            if (c == '*') {
                sv.remove_prefix(1);
            } else {
                while (!sv.empty() && (std::isalnum(static_cast<unsigned char>(sv.front()))
                                       || sv.front() == '-' || sv.front() == '_')) {
                    char ch = sv.front();
                    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
                    tag.push_back(ch);
                    sv.remove_prefix(1);
                }
                ss.tag = std::move(tag);
            }
            any = true;
        } else {
            break;
        }
    }
    if (!any) throw std::runtime_error("empty simple selector");
    return ss;
}

}  // namespace

Selector Selector::compile(std::string_view source) {
    Selector sel;
    std::string_view sv = source;
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) sv.remove_prefix(1);
    while (!sv.empty()) {
        Combinator combinator = Combinator::Descendant;
        if (sv.front() == '>') {
            combinator = Combinator::Child;
            sv.remove_prefix(1);
        }
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\n'))
            sv.remove_prefix(1);
        if (sv.empty()) break;
        SimpleSelector ss = parse_simple(sv);
        sel.steps_.push_back({combinator, std::move(ss)});
        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\n'))
            sv.remove_prefix(1);
    }
    if (sel.steps_.empty()) {
        throw std::runtime_error("empty selector: " + std::string{source});
    }
    return sel;
}

std::vector<Node*> Selector::select_all(Node& root) const {
    std::vector<Node*> current{&root};
    for (std::size_t i = 0; i < steps_.size(); ++i) {
        const auto& step = steps_[i];
        std::vector<Node*> next;
        for (Node* parent : current) {
            if (step.combinator == Combinator::Child) {
                for (auto& c : parent->children) {
                    if (step.simple.matches(*c)) next.push_back(c.get());
                }
            } else {
                walk_descendants(*parent, step.simple, next);
            }
        }
        current = std::move(next);
    }
    return current;
}

Node* Selector::select_first(Node& root) const {
    auto r = select_all(root);
    return r.empty() ? nullptr : r.front();
}

Node* select_first(Node& root, std::string_view selector) {
    return Selector::compile(selector).select_first(root);
}

Node& select(Node& root, std::string_view selector) {
    auto* n = Selector::compile(selector).select_first(root);
    if (!n) throw std::runtime_error("no match for selector: " + std::string{selector});
    return *n;
}

std::vector<Node*> select_all(Node& root, std::string_view selector) {
    return Selector::compile(selector).select_all(root);
}

}  // namespace qualbum::html
