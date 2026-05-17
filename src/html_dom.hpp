#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace qualbum::html {

enum class NodeKind {
    Document,
    Element,
    Text,
    Comment,
    Doctype,
    ProcessingInstruction,
    CData,
};

struct Attr {
    std::string name;
    std::string value;
};

class Node {
public:
    NodeKind kind = NodeKind::Element;
    std::string tag;           // lowercased for HTML; original case for XML
    std::string text;          // for Text/Comment/Doctype/PI/CData
    std::vector<Attr> attrs;
    std::vector<std::unique_ptr<Node>> children;
    Node* parent = nullptr;
    bool xml_mode = false;
    bool self_closing = false; // XML mode hint

    // Attribute helpers (linear scan — attribute lists are small).
    const std::string* attr(std::string_view name) const;
    std::string get_attr(std::string_view name,
                         std::string_view def = std::string_view{}) const;
    void set_attr(std::string_view name, std::string value);
    bool has_attr(std::string_view name) const;
    bool has_class(std::string_view cls) const;

    // Child manipulation.
    void clear_children();
    Node* append(std::unique_ptr<Node> child);
    Node* append_element(std::string_view tag);
    Node* append_text(std::string content);
    void set_text(std::string content);   // replaces all children
    std::unique_ptr<Node> clone_deep() const;
};

struct ParseOptions {
    bool xml = false;
};

std::unique_ptr<Node> parse(std::string_view source, ParseOptions opts = {});

// Serialize a node and its subtree as HTML (or XML if xml_mode was set).
void serialize(const Node& root, std::string& out);
std::string serialize(const Node& root);

// CSS selector subset:
//   tag, #id, .class, tag.class, tag#id
//   descendant ' ', child '>'
//   attribute equality [name="value"]
struct SimpleSelector {
    std::optional<std::string> tag;
    std::optional<std::string> id;
    std::vector<std::string> classes;
    std::vector<std::pair<std::string, std::string>> attr_eq;

    bool matches(const Node& n) const;
};

enum class Combinator { Descendant, Child };

struct SelectorStep {
    Combinator combinator;  // combinator linking the *previous* compound to this one
    SimpleSelector simple;
};

class Selector {
public:
    static Selector compile(std::string_view source);
    std::vector<Node*> select_all(Node& root) const;
    Node* select_first(Node& root) const;

private:
    std::vector<SelectorStep> steps_;
};

// One-shot helpers. select(): throws if no match. select_first(): returns null.
Node* select_first(Node& root, std::string_view selector);
Node& select(Node& root, std::string_view selector);
std::vector<Node*> select_all(Node& root, std::string_view selector);

}  // namespace qualbum::html
