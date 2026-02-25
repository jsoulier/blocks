#include <SDL3/SDL.h>

#include "physics.h"

static const float PHYSICS_EPSILON = 0.001f;


static void resolve_step(const physics_aabb_t* aabb, float* x, float* y, float* z, int axis, float step, physics_is_solid_fn is_solid)
{
    // Step-by-step AABB provides a bit more accuracy
    float start[3] = {*x, *y, *z};
    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 8; i++)
    {
        float t = (low + high) * 0.5f;
        float next[3] = {start[0], start[1], start[2]};
        next[axis] += step * t;
        if (physics_is_colliding(aabb, next[0], next[1], next[2], is_solid))
        {
            high = t;
        }
        else
        {
            low = t;
        }
    }
    if (axis == 0)
    {
        *x = start[0] + step * low;
    }
    else if (axis == 1)
    {
        *y = start[1] + step * low;
    }
    else
    {
        *z = start[2] + step * low;
    }
}


bool physics_is_colliding(const physics_aabb_t* aabb, float x, float y, float z, physics_is_solid_fn is_solid)
{
    int min_x = SDL_floorf(x + aabb->min_x + PHYSICS_EPSILON);
    int max_x = SDL_floorf(x + aabb->max_x - PHYSICS_EPSILON);
    int min_y = SDL_floorf(y + aabb->min_y + PHYSICS_EPSILON);
    int max_y = SDL_floorf(y + aabb->max_y - PHYSICS_EPSILON);
    int min_z = SDL_floorf(z + aabb->min_z + PHYSICS_EPSILON);
    int max_z = SDL_floorf(z + aabb->max_z - PHYSICS_EPSILON);
    for (int bx = min_x; bx <= max_x; bx++)
    {
        for (int by = min_y; by <= max_y; by++)
        {
            for (int bz = min_z; bz <= max_z; bz++)
            {
                if (is_solid(bx, by, bz)) return true;
            }
        }
    }
    return false;
}

bool physics_move_axis(const physics_aabb_t* aabb, float* x, float* y, float* z, int axis, float delta, float step_size, physics_is_solid_fn is_solid)
{
    // Tiny jitter check prevent small float issues and additional unnecessary collision work.
    if (SDL_fabsf(delta) <= SDL_FLT_EPSILON)
    {
        return false;
    }
    int steps = (int) SDL_ceilf(SDL_fabsf(delta) / step_size);
    steps = SDL_max(steps, 1);
    float step = delta / steps;
    for (int i = 0; i < steps; i++)
    {
        float next_x = (*x) + ((axis == 0)? step : 0);
        float next_y = (*y) + ((axis == 1)? step : 0);
        float next_z = (*z) + ((axis == 2)? step : 0);

        if (physics_is_colliding(aabb, next_x, next_y, next_z, is_solid))
        {
            resolve_step(aabb, x, y, z, axis, step, is_solid);
            return true;
        }

        *x = next_x;
        *y = next_y;
        *z = next_z;
    }
    return false;
}

bool physics_overlaps_block(const physics_aabb_t* aabb, float x, float y, float z, const int block_position[3])
{
    float aabb_min_x = x + aabb->min_x + PHYSICS_EPSILON;
    float aabb_max_x = x + aabb->max_x - PHYSICS_EPSILON;
    float aabb_min_y = y + aabb->min_y + PHYSICS_EPSILON;
    float aabb_max_y = y + aabb->max_y - PHYSICS_EPSILON;
    float aabb_min_z = z + aabb->min_z + PHYSICS_EPSILON;
    float aabb_max_z = z + aabb->max_z - PHYSICS_EPSILON;
    float block_min_x = block_position[0];
    float block_max_x = block_position[0] + 1.0f;
    float block_min_y = block_position[1];
    float block_max_y = block_position[1] + 1.0f;
    float block_min_z = block_position[2];
    float block_max_z = block_position[2] + 1.0f;
    return
        (aabb_max_x > block_min_x && aabb_min_x < block_max_x) &&
        (aabb_max_y > block_min_y && aabb_min_y < block_max_y) &&
        (aabb_max_z > block_min_z && aabb_min_z < block_max_z);
}
