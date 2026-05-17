#include "test_main.hpp"

#include "markdown.hpp"

using qualbum::md::render;

// Reference outputs captured from mistune 0.8.4. See
// tools/capture_baselines.py for the script that regenerates these.

TEST_CASE("markdown: empty input") {
    REQUIRE_EQ(render(""), std::string{""});
}

TEST_CASE("markdown: single paragraph") {
    REQUIRE_EQ(render("Just one paragraph with no formatting."),
               std::string{"<p>Just one paragraph with no formatting.</p>\n"});
}

TEST_CASE("markdown: two paragraphs") {
    const char* in =
        "Harvest Moon - October 4, 2017 at 10:07 PM. I attempted earlier in the "
        "evening to capture the Harvest Moon between an opening in the trees but it "
        "was too cloudy.\n"
        "\n"
        "Lens - AF-S DX NIKKOR 55-200mm f/4-5.6G VR II @ 200mm on Nikon D5500";
    const char* expected =
        "<p>Harvest Moon - October 4, 2017 at 10:07 PM. I attempted earlier in the "
        "evening to capture the Harvest Moon between an opening in the trees but it "
        "was too cloudy.</p>\n"
        "<p>Lens - AF-S DX NIKKOR 55-200mm f/4-5.6G VR II @ 200mm on Nikon D5500</p>\n";
    REQUIRE_EQ(render(in), std::string{expected});
}

TEST_CASE("markdown: bold, italic, code") {
    REQUIRE_EQ(render("This is **bold** and *italic* and ***both***."),
               std::string{"<p>This is <strong>bold</strong> and <em>italic</em> "
                           "and <strong><em>both</em></strong>.</p>\n"});
    REQUIRE_EQ(render("Use the `config` value carefully."),
               std::string{"<p>Use the <code>config</code> value carefully.</p>\n"});
}

TEST_CASE("markdown: link") {
    REQUIRE_EQ(render("Visit [Anthropic](https://anthropic.com) for more."),
               std::string{"<p>Visit <a href=\"https://anthropic.com\">Anthropic</a> for more.</p>\n"});
}

TEST_CASE("markdown: autolinks (URL + email)") {
    REQUIRE_EQ(render("Email <user@example.com> or visit <https://example.com>."),
               std::string{"<p>Email <a href=\"mailto:user@example.com\">user@example.com</a> "
                           "or visit <a href=\"https://example.com\">https://example.com</a>.</p>\n"});
}

TEST_CASE("markdown: hard line break") {
    REQUIRE_EQ(render("line one  \nline two"),
               std::string{"<p>line one<br>\nline two</p>\n"});
}

TEST_CASE("markdown: mixed inline across multiple paragraphs") {
    const char* in =
        "First paragraph with *emphasis*.\n"
        "\n"
        "Second paragraph with **strong** text.\n"
        "\n"
        "Third with [a link](https://a.b/).";
    const char* expected =
        "<p>First paragraph with <em>emphasis</em>.</p>\n"
        "<p>Second paragraph with <strong>strong</strong> text.</p>\n"
        "<p>Third with <a href=\"https://a.b/\">a link</a>.</p>\n";
    REQUIRE_EQ(render(in), std::string{expected});
}

TEST_CASE("markdown: escaped chars are literal, ampersands escape to entities") {
    REQUIRE_EQ(render("Use \\*not italic\\* here."),
               std::string{"<p>Use *not italic* here.</p>\n"});
    REQUIRE_EQ(render("Tom & Jerry"),
               std::string{"<p>Tom &amp; Jerry</p>\n"});
}
