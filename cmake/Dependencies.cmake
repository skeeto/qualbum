include(ExternalProject)

set(LIBJPEG_TURBO_VERSION "3.0.4")
set(LIBJPEG_TURBO_URL
    "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEG_TURBO_VERSION}/libjpeg-turbo-${LIBJPEG_TURBO_VERSION}.tar.gz")
set(LIBJPEG_TURBO_SHA256
    "99130559e7d62e8d695f2c0eaeef912c5828d5b84a0537dcb24c9678c9d5b76b")

set(_jpeg_prefix     "${CMAKE_BINARY_DIR}/libjpeg-turbo")
set(_jpeg_install    "${_jpeg_prefix}/install")
set(_jpeg_include    "${_jpeg_install}/include")
set(_jpeg_lib_dir    "${_jpeg_install}/lib")

# CMake needs the include directory to exist at configure time so that an
# imported target with INTERFACE_INCLUDE_DIRECTORIES validates.
file(MAKE_DIRECTORY "${_jpeg_include}")

# libjpeg-turbo only renames the static lib to plain `jpeg` on non-MSVC
# toolchains (its CMakeLists.txt:671). MSVC keeps the target name verbatim:
#   MSVC                    -> jpeg-static.lib
#   MinGW (w64devkit)       -> libjpeg.a   (Unix prefix+suffix)
#   Linux/macOS (GCC/Clang) -> libjpeg.a
if(MSVC)
    set(_jpeg_lib_path "${_jpeg_lib_dir}/jpeg-static.lib")
else()
    set(_jpeg_lib_path "${_jpeg_lib_dir}/libjpeg.a")
endif()

set(_jpeg_simd ON)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|x86_64|AMD64|i.86")
    set(_jpeg_require_simd ON)
else()
    set(_jpeg_require_simd OFF)
endif()

set(_jpeg_cmake_args
    "-DCMAKE_INSTALL_PREFIX=${_jpeg_install}"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
    "-DENABLE_SHARED=OFF"
    "-DENABLE_STATIC=ON"
    "-DWITH_TURBOJPEG=OFF"
    "-DWITH_JAVA=OFF"
    "-DWITH_SIMD=${_jpeg_simd}"
    "-DREQUIRE_SIMD=${_jpeg_require_simd}"
)

ExternalProject_Add(libjpeg_turbo_ext
    URL              "${LIBJPEG_TURBO_URL}"
    URL_HASH         SHA256=${LIBJPEG_TURBO_SHA256}
    PREFIX           "${_jpeg_prefix}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS       ${_jpeg_cmake_args}
    BUILD_BYPRODUCTS "${_jpeg_lib_path}"
    UPDATE_DISCONNECTED TRUE
)

add_library(jpeg-static STATIC IMPORTED GLOBAL)
add_dependencies(jpeg-static libjpeg_turbo_ext)
set_target_properties(jpeg-static PROPERTIES
    IMPORTED_LOCATION             "${_jpeg_lib_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${_jpeg_include}"
)
