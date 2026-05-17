#pragma once

#include "config.hpp"

namespace qualbum {

// Generate the static site under cfg.destination based on the current working
// directory. Mirrors qualbum.py's `generate()`.
int run(const Config& cfg);

}  // namespace qualbum
