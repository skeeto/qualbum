#include "test_main.hpp"

#include "html_dom.hpp"

using qualbum::html::parse;
using qualbum::html::serialize;
using qualbum::html::ParseOptions;
using qualbum::html::Node;
using qualbum::html::NodeKind;
using qualbum::html::select;
using qualbum::html::select_first;
using qualbum::html::select_all;

TEST_CASE("html: round-trip basic doc") {
    auto doc = parse("<!DOCTYPE html>\n<html><head><title>X</title></head>\n"
                     "<body><p class=\"a\">hi</p></body></html>");
    auto s = serialize(*doc);
    REQUIRE(s.find("<!DOCTYPE html>") != std::string::npos);
    REQUIRE(s.find("<title>X</title>") != std::string::npos);
    REQUIRE(s.find("<p class=\"a\">hi</p>") != std::string::npos);
}

TEST_CASE("html: void elements emit without close tag") {
    auto doc = parse("<head><meta http-equiv=\"X\" content=\"Y\">"
                     "<link rel=\"stylesheet\" href=\"main.css\"></head>");
    auto s = serialize(*doc);
    REQUIRE(s.find("<meta http-equiv=\"X\" content=\"Y\">") != std::string::npos);
    REQUIRE(s.find("<link rel=\"stylesheet\" href=\"main.css\">") != std::string::npos);
    REQUIRE(s.find("</meta>") == std::string::npos);
    REQUIRE(s.find("</link>") == std::string::npos);
}

TEST_CASE("html: self-closing void <br/> normalized") {
    auto doc = parse("<p>line<br/>break</p>");
    auto s = serialize(*doc);
    REQUIRE(s.find("<br>") != std::string::npos);
    REQUIRE(s.find("</br>") == std::string::npos);
}

TEST_CASE("html: script content preserved verbatim") {
    const char* src =
        "<script>\n"
        "if (a < b && c > d) { foo(); }\n"
        "</script>";
    auto doc = parse(src);
    auto s = serialize(*doc);
    REQUIRE(s.find("if (a < b && c > d)") != std::string::npos);
}

TEST_CASE("html: entity decoding and re-encoding") {
    auto doc = parse("<p>A &amp; B &lt; C</p>");
    auto p = select_first(*doc, "p");
    REQUIRE(p != nullptr);
    REQUIRE_EQ(p->children.size(), 1u);
    REQUIRE_EQ(p->children[0]->text, std::string{"A & B < C"});
    auto s = serialize(*doc);
    REQUIRE(s.find("A &amp; B &lt; C") != std::string::npos);
}

TEST_CASE("selector: id, class, tag") {
    auto doc = parse("<div id=\"a\"><span class=\"x y\">hi</span>"
                     "<span class=\"x\">there</span></div>");
    REQUIRE(select_first(*doc, "#a") != nullptr);
    REQUIRE_EQ(select_all(*doc, ".x").size(), 2u);
    REQUIRE_EQ(select_all(*doc, ".y").size(), 1u);
    REQUIRE_EQ(select_all(*doc, "span").size(), 2u);
}

TEST_CASE("selector: descendant and child combinators") {
    auto doc = parse(
        "<feed>"
            "<title>hi</title>"
            "<entry><title>nested</title></entry>"
        "</feed>");
    auto descendants = select_all(*doc, "feed title");
    REQUIRE_EQ(descendants.size(), 2u);
    auto children = select_all(*doc, "feed > title");
    REQUIRE_EQ(children.size(), 1u);
    REQUIRE_EQ(children[0]->children[0]->text, std::string{"hi"});
}

TEST_CASE("selector: attribute equality") {
    auto doc = parse(
        "<feed>"
            "<link rel=\"self\" href=\"a\"/>"
            "<link rel=\"alternate\" href=\"b\"/>"
        "</feed>",
        ParseOptions{true});
    auto self = select_first(*doc, "feed link[rel=\"self\"]");
    REQUIRE(self != nullptr);
    REQUIRE_EQ(self->get_attr("href"), std::string{"a"});
}

TEST_CASE("node ops: set_text replaces children, set_attr in place") {
    auto doc = parse("<h1 id=\"title\">old</h1>");
    Node* h = select_first(*doc, "#title");
    REQUIRE(h != nullptr);
    h->set_text("new");
    h->set_attr("class", "big");
    auto s = serialize(*doc);
    REQUIRE(s.find("<h1 id=\"title\" class=\"big\">new</h1>") != std::string::npos);
}

TEST_CASE("node ops: append element and append_text") {
    auto doc = parse("<ul id=\"g\"></ul>");
    auto* ul = select_first(*doc, "#g");
    auto* li = ul->append_element("li");
    li->append_text("first");
    auto* li2 = ul->append_element("li");
    li2->append_text("second");
    auto s = serialize(*doc);
    REQUIRE(s.find("<ul id=\"g\"><li>first</li><li>second</li></ul>") != std::string::npos);
}

TEST_CASE("xml mode: self-close empty, preserve PI") {
    auto doc = parse(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\">"
            "<link rel=\"self\" type=\"application/atom+xml\"/>"
            "<title></title>"
        "</feed>",
        ParseOptions{true});
    auto s = serialize(*doc);
    REQUIRE(s.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") != std::string::npos);
    REQUIRE(s.find("<link rel=\"self\" type=\"application/atom+xml\"/>") != std::string::npos);
    REQUIRE(s.find("<title/>") != std::string::npos);
}

TEST_CASE("xml mode: text escapes < > & not '") {
    auto doc = parse("<a></a>", ParseOptions{true});
    auto* a = select_first(*doc, "a");
    a->set_text("<p>he said \"hi\" & bye</p>");
    auto s = serialize(*doc);
    REQUIRE(s.find("&lt;p&gt;he said \"hi\" &amp; bye&lt;/p&gt;") != std::string::npos);
}

TEST_CASE("html: parses the actual _gallery.html template") {
    const char* tmpl =
        "<!DOCTYPE html>\n"
        "<html class=\"gallery\" lang=\"en\">\n"
        "    <head>\n"
        "        <title></title>\n"
        "        <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"/>\n"
        "        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n"
        "        <link rel=\"alternate\" type=\"application/atom+xml\" href=\"feed/\"/>\n"
        "        <link rel=\"stylesheet\" class=\"root-href\" href=\"/main.css\"/>\n"
        "    </head>\n"
        "    <body class=\"gallery\">\n"
        "        <header>\n"
        "            <h1 id=\"title\"></h1>\n"
        "        </header>\n"
        "        <ul id=\"gallery\"></ul>\n"
        "        <nav class=\"page\">\n"
        "            <a href=\"#\" title=\"Previous page\" id=\"prev\" class=\"nav prev\"></a>\n"
        "            <a href=\"#\" title=\"Next page\" id=\"next\" class=\"nav next\"></a>\n"
        "        </nav>\n"
        "    </body>\n"
        "</html>\n";
    auto doc = parse(tmpl);
    REQUIRE(select_first(*doc, "title") != nullptr);
    REQUIRE(select_first(*doc, "#title") != nullptr);
    REQUIRE(select_first(*doc, "#gallery") != nullptr);
    REQUIRE(select_first(*doc, "#prev") != nullptr);
    REQUIRE(select_first(*doc, "#next") != nullptr);
    auto root_hrefs = select_all(*doc, ".root-href");
    REQUIRE(!root_hrefs.empty());
}

TEST_CASE("selector: [href=\"feed/\"] finds both gallery feed links") {
    // Used by the paginated-page fixer in gallery.cpp: the head <link
    // rel="alternate"> and the body's <a>feed</a>.
    const char* tmpl =
        "<!DOCTYPE html>\n"
        "<html>\n"
        "  <head>\n"
        "    <link rel=\"alternate\" type=\"application/atom+xml\" href=\"feed/\"/>\n"
        "  </head>\n"
        "  <body>\n"
        "    <ul>\n"
        "      <li class=\"icon home\"><a href=\"/\">home</a></li>\n"
        "      <li class=\"icon feed\"><a href=\"feed/\">feed</a></li>\n"
        "    </ul>\n"
        "  </body>\n"
        "</html>\n";
    auto doc = parse(tmpl);
    auto matches = select_all(*doc, "[href=\"feed/\"]");
    REQUIRE_EQ(matches.size(), 2u);
    // Rewriting the attribute in place should leave the home/href alone.
    for (auto* el : matches) el->set_attr("href", "../feed/");
    auto post = select_all(*doc, "[href=\"feed/\"]");
    REQUIRE(post.empty());
    auto rewritten = select_all(*doc, "[href=\"../feed/\"]");
    REQUIRE_EQ(rewritten.size(), 2u);
    REQUIRE(select_first(*doc, "[href=\"/\"]") != nullptr);
}
