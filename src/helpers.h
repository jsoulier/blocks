#pragma once

#include <SDL3/SDL.h>
#include <assert.h>

#undef assert
#undef min
#undef max

#define EPSILON 0.000001
#define PI 3.14159265359
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define clamp(x, a, b) min(b, max(a, x))
#define deg(rad) ((rad) * 180.0 / PI)
#define rad(deg) ((deg) * PI / 180.0)
#define abs(x) ((x) > 0 ? (x) : -(x))

#ifndef NDEBUG
#define assert(e) SDL_assert_always(e)
#else
#define assert(e)
#endif

typedef enum
{
    DIRECTION_N,
    DIRECTION_S,
    DIRECTION_E,
    DIRECTION_W,
    DIRECTION_U,
    DIRECTION_D,
    DIRECTION_2 = 4,
    DIRECTION_3 = 6,
}
direction_t;

extern const int directions[][3];

void sort_2d(
    const int x,
    const int z,
    void* data,
    const int size);