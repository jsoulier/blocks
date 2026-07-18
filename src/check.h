#pragma once

#include <SDL3/SDL.h>

// TODO: remove this file. replace CHECK with SDL_assert and add appropriate SDL defs in CMakeLists to remove in Release builds
#ifndef NDEBUG
#define CHECK(x) SDL_assert_always(x)
#else
#define CHECK(x)
#endif
