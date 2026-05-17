add_executable(bench_resize bench_resize.cpp)
target_link_libraries(bench_resize PRIVATE qualbum_lib)
target_include_directories(bench_resize PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(bench_resize PRIVATE
    QUALBUM_GALLERY_ROOT="${CMAKE_SOURCE_DIR}/.."
)
