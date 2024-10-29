#include <stdbool.h>
#include "block.h"
#include "helpers.h"
#include "physics.h"
#include "world.h"

bool physics_raycast(
    float* x,
    float* y,
    float* z,
    const float dx,
    const float dy,
    const float dz,
    const float step,
    const float length,
    const bool previous)
{
    assert(x);
    assert(y);
    assert(z);
    for (float i = 0; i < length; i += step) {
        float a = *x + dx * i;
        float b = *y + dy * i;
        float c = *z + dz * i;
        // TODO: why?
        if (a <= 0) {
            a -= 1.0f;
        }
        if (c <= 0) {
            c -= 1.0f;
        }
        if (block_collision(world_get_block(a, b, c))) {
            if (previous) {
                a -= dx * step;
                b -= dy * step;
                c -= dz * step;
            }
            // TODO: why?
            if (a < 0.0f && a > -1.0f && dx > 0.0f) {
                a = -1.0f;
            }
            if (c < 0.0f && c > -1.0f && dz > 0.0f) {
                c = -1.0f;
            }
            *x = a;
            *y = b;
            *z = c;
            return true;
        }
    }
    return false;
}