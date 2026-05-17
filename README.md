# Quick Album Generator

Qualbum is a C++ program that generates a static photo album website from
a collection of images. Images are be grouped into galleries with other
images in the same directory.

* [**Demo Gallery**](http://nullprogram.com/qualbum/)

Except for the root "gallery" that contains every image, each gallery must
have a `_gallery.yaml` in its directory with `title` and optional `image`
fields. Galleries are sorted newest first. Each gallery gets its own Atom
feed and is will appear in an albums listing.

Each image must be accompanied by a Markdown `.md` file with the same
name. This file has a YAML "front matter" providing basic metadata for the
image (title, date, etc.), and is followed by a free-form description of
the image in Markdown.

The "Favorite Colors" (`colors/`) gallery in serves as a basic example.

## Build

Requires CMake and a C++23 compiler with GCC vector extensions:

    $ cmake -B build
    $ cmake --build build

Supports Windows (i.e. w64devkit), Linux, and macOS. The `install` target
installs the binary and a man page. If you need to run Qualbum on a very
old Linux production system, then static link everything:

    $ cmake -B build -DCMAKE_EXE_LINKER_FLAGS=-static

## Generate

    $ qualbum

See `--help` or the man page for a full option list.
