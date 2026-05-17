#include "test_main.hpp"

#include "uuid_v3.hpp"

TEST_CASE("uuid_v3: NAMESPACE_URL matches Python uuid.uuid3") {
    using qualbum::uuidv3::from_name_str;
    using qualbum::uuidv3::NAMESPACE_URL;

    REQUIRE_EQ(from_name_str(NAMESPACE_URL, ""),
               std::string{"14cdb9b4-de01-3faa-aff5-65bc2f771745"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL, "a"),
               std::string{"33838e93-63bc-3732-bf9e-b68d970b79c2"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL, "abc"),
               std::string{"874a8cb4-4e91-3055-a476-3d3e2ffe375f"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL, "http://example.com/"),
               std::string{"773536a8-4b7b-383d-9106-697d4d366254"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL,
                             "http://photo.nullprogram.com/space/"),
               std::string{"e9540dc9-c239-398f-b78e-d64ea4c82adb"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL,
                             "http://photo.nullprogram.com/birds/heron03/"),
               std::string{"ecf3a86f-d775-32a7-83c6-bb46a71dc8b0"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL, "message digest"),
               std::string{"711bce96-aa35-35c2-b19b-d154033b1e25"});
    REQUIRE_EQ(from_name_str(NAMESPACE_URL,
                             "The quick brown fox jumps over the lazy dog"),
               std::string{"21e7e532-eaaa-3124-9e8c-175552ecba5b"});
}

TEST_CASE("uuid_v3: version=3 and RFC 4122 variant bits") {
    using namespace qualbum::uuidv3;
    auto u = from_name(NAMESPACE_URL, "anything");
    // Version nibble is byte 6, top 4 bits = 3
    REQUIRE_EQ((u[6] >> 4) & 0xf, 3);
    // Variant top two bits of byte 8 = 10b
    REQUIRE_EQ((u[8] >> 6) & 0x3, 2);
}
