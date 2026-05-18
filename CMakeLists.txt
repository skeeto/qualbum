cmake_minimum_required(VERSION 3.24)
project(qualbum LANGUAGES C CXX VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

include(cmake/Warnings.cmake)
include(cmake/Tuning.cmake)
include(cmake/Dependencies.cmake)

find_package(Threads REQUIRED)

set(QUALBUM_LIB_SOURCES
    src/app.cpp
    src/config.cpp
    src/yaml_subset.cpp
    src/markdown.cpp
    src/html_dom.cpp
    src/md5.cpp
    src/uuid_v3.cpp
    src/datetime.cpp
    src/srgb.cpp
    src/image_io.cpp
    src/image_resize.cpp
    src/thread_pool.cpp
    src/fs_util.cpp
    src/string_util.cpp
    src/photo.cpp
    src/gallery.cpp
    src/feed.cpp
)

add_library(qualbum_lib STATIC ${QUALBUM_LIB_SOURCES})
target_include_directories(qualbum_lib PUBLIC src)
target_link_libraries(qualbum_lib
    PUBLIC qualbum_warnings qualbum_tuning
    PRIVATE Threads::Threads jpeg-static
)

# Image scaling is the dominant cost of every full build. Push it harder than
# the rest of the library: -O3 + loop unrolling lets the compiler keep more
# pixels in flight through the SIMD pipeline, and -ffp-contract=fast enables
# fused multiply-add fusion across statements (which `fmla v0.4s,...` needs).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set_source_files_properties(src/image_resize.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<CONFIG:Release>:-O3;-funroll-loops;-ffp-contract=fast>"
    )
endif()

add_executable(qualbum src/main.cpp)
target_link_libraries(qualbum PRIVATE qualbum_lib)
target_compile_definitions(qualbum PRIVATE QUALBUM_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_sources(qualbum PRIVATE src/platform/windows_manifest.rc)
endif()

include(GNUInstallDirs)
install(TARGETS qualbum
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES doc/qualbum.1
        DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

enable_testing()
add_subdirectory(tests)

option(BUILD_BENCH "Build resize/perf benchmarks" ON)
if(BUILD_BENCH)
    add_subdirectory(bench)
endif()
