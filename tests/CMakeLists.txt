set(QUALBUM_TEST_SOURCES
    test_main.cpp
    test_md5.cpp
    test_uuid_v3.cpp
    test_string_util.cpp
    test_srgb.cpp
    test_datetime.cpp
    test_fs_util.cpp
    test_yaml_subset.cpp
    test_html_dom.cpp
    test_markdown.cpp
    test_resize.cpp
    test_thread_pool.cpp
)

add_executable(qualbum_tests ${QUALBUM_TEST_SOURCES})
target_link_libraries(qualbum_tests PRIVATE qualbum_lib)
target_include_directories(qualbum_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(qualbum_tests PRIVATE
    QUALBUM_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QUALBUM_GALLERY_ROOT="${CMAKE_SOURCE_DIR}/.."
)

add_test(NAME qualbum_unit_tests COMMAND qualbum_tests)
set_tests_properties(qualbum_unit_tests PROPERTIES
    FAIL_REGULAR_EXPRESSION "FAILED;ERROR;Assertion failed"
)
