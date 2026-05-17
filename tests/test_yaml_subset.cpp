#include "test_main.hpp"

#include "yaml_subset.hpp"

using qualbum::yamlx::parse_file;
using qualbum::yamlx::parse_frontmatter;
using qualbum::yamlx::ParseError;

TEST_CASE("yaml_subset: bare strings preserved verbatim") {
    auto m = parse_file(
        "title: Snowy Bench\n"
        "iso: ISO100\n"
        "f-stop: f/5.6\n"
        "exposure-time: 1/3 sec\n");
    REQUIRE_EQ(m.get_string("title"), std::string{"Snowy Bench"});
    REQUIRE_EQ(m.get_string("iso"), std::string{"ISO100"});
    REQUIRE_EQ(m.get_string("f-stop"), std::string{"f/5.6"});
    REQUIRE_EQ(m.get_string("exposure-time"), std::string{"1/3 sec"});
}

TEST_CASE("yaml_subset: quoted strings keep specials") {
    auto m = parse_file(
        "title: \"B & O Railroad\"\n"
        "key: \"Calvin: July 9, 2008 - February 18, 2025\"\n");
    REQUIRE_EQ(m.get_string("title"), std::string{"B & O Railroad"});
    REQUIRE_EQ(m.get_string("key"), std::string{"Calvin: July 9, 2008 - February 18, 2025"});
}

TEST_CASE("yaml_subset: integers vs prefixed strings") {
    auto m = parse_file(
        "gravity: 60\n"
        "pagemax: 24\n"
        "negative: -5\n");
    REQUIRE_EQ(m.get_int("gravity"), 60);
    REQUIRE_EQ(m.get_int("pagemax"), 24);
    REQUIRE_EQ(m.get_int("negative"), -5);
}

TEST_CASE("yaml_subset: timestamps with offsets and Z") {
    auto m = parse_file(
        "a: 2017-10-07T19:38:00-04:00\n"
        "b: 2017-07-28T00:00:00Z\n"
        "c: 2024-10-12T12:29:00-04:00\n");
    auto a = m.try_get_timestamp("a");
    REQUIRE(a.has_value());
    REQUIRE_EQ(a->year, 2017);
    REQUIRE_EQ(a->tz_offset_min, -240);
    auto b = m.try_get_timestamp("b");
    REQUIRE(b.has_value());
    REQUIRE(b->had_z_marker);
    auto c = m.try_get_timestamp("c");
    REQUIRE(c.has_value());
    REQUIRE_EQ(c->year, 2024);
}

TEST_CASE("yaml_subset: empty value is null/empty string") {
    auto m = parse_file("image:\n");
    REQUIRE(m.contains("image"));
    REQUIRE_EQ(m.get_string("image"), std::string{""});
    REQUIRE_EQ(m.get_string("image", "fallback"), std::string{"fallback"});
}

TEST_CASE("yaml_subset: blank and comment lines tolerated") {
    auto m = parse_file(
        "\n"
        "# this is a comment\n"
        "title: Hello\n"
        "\n"
        "author: Me\n");
    REQUIRE_EQ(m.get_string("title"), std::string{"Hello"});
    REQUIRE_EQ(m.get_string("author"), std::string{"Me"});
}

TEST_CASE("yaml_subset: rejects unsupported features") {
    REQUIRE_THROWS(parse_file("- 1\n- 2\n"));
    REQUIRE_THROWS(parse_file("nested:\n  key: value\n"));
    REQUIRE_THROWS(parse_file("flow: [a, b]\n"));
    REQUIRE_THROWS(parse_file("anchor: &foo bar\n"));
    REQUIRE_THROWS(parse_file("missing_colon foo\n"));
}

TEST_CASE("yaml_subset: frontmatter splits at second ---") {
    auto fm = parse_frontmatter(
        "---\n"
        "title: \"Harvest Moon\"\n"
        "date: 2017-10-07T19:38:00-04:00\n"
        "iso: ISO100\n"
        "---\n"
        "Paragraph one.\n"
        "\n"
        "Paragraph two.\n");
    REQUIRE_EQ(fm.meta.get_string("title"), std::string{"Harvest Moon"});
    REQUIRE_EQ(fm.meta.get_string("iso"), std::string{"ISO100"});
    REQUIRE_EQ(fm.body, std::string{"Paragraph one.\n\nParagraph two.\n"});
}

TEST_CASE("yaml_subset: frontmatter requires closing fence") {
    REQUIRE_THROWS(parse_frontmatter("---\ntitle: x\n"));
    REQUIRE_THROWS(parse_frontmatter("title: x\n---\nbody\n"));  // missing opening
}

TEST_CASE("yaml_subset: real _config.yaml shape") {
    auto m = parse_file(
        "title: Kelsey Wellons Photography\n"
        "author: Kelsey Wellons\n"
        "baseurl: http://photo.nullprogram.com\n"
        "pagemax: 24\n");
    REQUIRE_EQ(m.get_string("title"), std::string{"Kelsey Wellons Photography"});
    REQUIRE_EQ(m.get_string("author"), std::string{"Kelsey Wellons"});
    REQUIRE_EQ(m.get_string("baseurl"), std::string{"http://photo.nullprogram.com"});
    REQUIRE_EQ(m.get_int("pagemax"), 24);
}
