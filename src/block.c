#include "block.h"
#include "direction.h"

struct
{
    bool is_opaque;
    bool is_sprite;
    bool is_solid;
    bool is_occluded;
    bool has_shadow;
    int indices[6];
    light_t light;
}
static const BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_GRASS] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 2,  2,  2,  2,  1,  3}
    },
    [BLOCK_DIRT] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 3,  3,  3,  3,  3,  3}
    },
    [BLOCK_SAND] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 5,  5,  5,  5,  5,  5}
    },
    [BLOCK_SNOW] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 6,  6,  6,  6,  6,  6}
    },
    [BLOCK_STONE] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 4,  4,  4,  4,  4,  4}
    },
    [BLOCK_LOG] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = { 8,  8,  8,  8,  7,  7}
    },
    [BLOCK_LEAVES] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .is_occluded = true,
        .has_shadow = true,
        .indices = {10, 10, 10, 10, 10, 10}
    },
    [BLOCK_CLOUD] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_shadow = true,
        .indices = { 9,  9,  9,  9,  9,  9}
    },
    [BLOCK_BUSH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_shadow = false,
        .indices = {15, 15, 15, 15, 15, 15}
    },
    [BLOCK_BLUEBELL] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_shadow = false,
        .indices = {13, 13, 13, 13, 13, 13}
    },
    [BLOCK_DANDELION] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_shadow = false,
        .indices = {12, 12, 12, 12, 12, 12}
    },
    [BLOCK_ROSE] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_shadow = false,
        .indices = {11, 11, 11, 11, 11, 11}
    },
    [BLOCK_LAVENDER] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_shadow = false,
        .indices = {14, 14, 14, 14, 14, 14}
    },
    [BLOCK_WATER] =
    {
        .is_opaque = false,
        .is_sprite = false,
        .is_solid = false,
        .has_shadow = false,
        .indices = {16, 16, 16, 16, 16, 16}
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .indices = {17, 17, 17, 17, 17, 17},
        .has_shadow = false,
        .light = {255, 255, 0, 10},
    },
};

bool block_is_opaque(block_t block)
{
    return BLOCKS[block].is_opaque;
}

bool block_is_sprite(block_t block)
{
    return BLOCKS[block].is_sprite;
}

bool block_is_solid(block_t block)
{
    return BLOCKS[block].is_solid;
}

bool block_is_occluded(block_t block)
{
    return BLOCKS[block].is_occluded;
}

bool block_has_shadow(block_t block)
{
    return BLOCKS[block].has_shadow;
}

int block_get_index(block_t block, direction_t direction)
{
    return BLOCKS[block].indices[direction];
}

bool block_is_light(block_t block)
{
    return BLOCKS[block].light.radius > 0;
}

light_t block_get_light(block_t block)
{
    return BLOCKS[block].light;
}
