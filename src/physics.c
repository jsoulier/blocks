#include <stdbool.h>
#include "config.h"
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
    const float length,
    const bool previous)
{
    assert(x);
    assert(y);
    assert(z);
    for (float i = 0.0f; i < length; i += RAYCAST_STEP)
    {
        float a = *x + dx * i;
        float b = *y + dy * i;
        float c = *z + dz * i;
        // TODO: why?
        if (a <= 0.0f)
        {
            a -= 1.0f;
        }
        if (c <= 0.0f)
        {
            c -= 1.0f;
        }
        if (block_solid(world_get_block(a, b, c)))
        {
            if (previous)
            {
                a -= dx * RAYCAST_STEP;
                b -= dy * RAYCAST_STEP;
                c -= dz * RAYCAST_STEP;
            }
            // TODO: why?
            if (a < 0.0f && a > -1.0f && dx > 0.0f)
            {
                a = -1.0f;
            }
            if (c < 0.0f && c > -1.0f && dz > 0.0f)
            {
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