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
    for (float i = 0.0f; i < length; i += RAYCAST_STEP) {
        float a = *x + dx * i;
        float b = *y + dy * i;
        float c = *z + dz * i;
        // TODO: why?
        if (a <= 0.0f) {
            a -= 1.0f;
        }
        if (c <= 0.0f) {
            c -= 1.0f;
        }
        if (block_collision(world_get_block(a, b, c))) {
            if (previous) {
                a -= dx * RAYCAST_STEP;
                b -= dy * RAYCAST_STEP;
                c -= dz * RAYCAST_STEP;
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

void physics_collide(
    float* x1,
    float* y1,
    float* z1,
    const float x2,
    const float y2,
    const float z2)
{
    assert(x1);
    assert(y1);
    assert(z1);
    const float dx = x2 - *x1;
    const float dy = y2 - *y1;
    const float dz = z2 - *z1;
    float a, b, c;
    a = *x1;
    b = *y1;
    c = *z1;

    // TODO: maybe we should cast e.g. for north:
    // a, b, c - 1 to 0, 0, 1
    // essentially start the cast slightly behind the player

    if (dy < 0.0f && physics_raycast(&a, &b, &c, 0.0f, -1.0f, 0.0f, abs(dy), true)) {
        *y1 = b;
    } else if (dy > 0.0f && physics_raycast(&a, &b, &c, 0.0f, 1.0f, 0.0f, abs(dy), true)) {
        *y1 = b;
    } else {
        *y1 = y2;
    }
    a = *x1;
    b = *y1;
    c = *z1;
    if (dx < 0.0f && physics_raycast(&a, &b, &c, -1.0f, 0.0f, 0.0f, abs(dx), true)) {
        *x1 = a + 1;
    } else if (dx > 0.0f && physics_raycast(&a, &b, &c, 1.0f, 0.0f, 0.0f, abs(dx), true)) {
        *x1 = a + 1;
    } else {
        *x1 = x2;
    }
    a = *x1;
    b = *y1;
    c = *z1;
    if (dz < 0.0f && physics_raycast(&a, &b, &c, 0.0f, 0.0f, -1.0f, abs(dz), true)) {
        *z1 = c + 1.0f;
    } else if (dz > 0.0f && physics_raycast(&a, &b, &c, 0.0f, 0.0f, 1.0f, abs(dz), true)) {
        *z1 = c + 1.0f;
    } else {
        *z1 = z2;
    }
}