#pragma once

#include <SDL3/SDL.h>

typedef bool (*physics_is_solid_fn)(int x, int y, int z);

typedef struct physics_aabb
{
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;
}
physics_aabb_t;



bool physics_is_colliding(const physics_aabb_t* aabb, float x, float y, float z, physics_is_solid_fn is_solid);
bool physics_move_axis(const physics_aabb_t* aabb, float* x, float* y, float* z, int axis, float delta, float step_size, physics_is_solid_fn is_solid);
bool physics_overlaps_block(const physics_aabb_t* aabb, float x, float y, float z, const int block_position[3]);
