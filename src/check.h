#pragma once

#include <SDL3/SDL.h>

#ifndef NDEBUG
#define CHECK(x) SDL_assert_always(x)
#else
#define CHECK(x)
#endif
