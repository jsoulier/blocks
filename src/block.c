#include <stdbool.h>
#include "helpers.h"
#include "block.h"

bool block_opaque(const block_t block)
{
    assert(block < BLOCK_COUNT);
    switch (block)
    {
    case BLOCK_EMPTY:
    case BLOCK_GLASS:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}

bool block_solid(const block_t block)
{
    assert(block < BLOCK_COUNT);
    switch (block)
    {
    case BLOCK_CLOUD:
    case BLOCK_EMPTY:
    case BLOCK_LAVA:
    case BLOCK_ROSE:
    case BLOCK_TULIP:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}

bool block_sprite(const block_t block)
{
    assert(block < BLOCK_COUNT);
    switch (block)
    {
    case BLOCK_ROSE:
    case BLOCK_TULIP:
        return 1;
    }
    return 0;
}

const int blocks[][DIRECTION_3][2] =
{
    [BLOCK_CLOUD] =
    {
        [DIRECTION_E] = { 0, 0 },
        [DIRECTION_W] = { 0, 0 },
        [DIRECTION_N] = { 0, 0 },
        [DIRECTION_S] = { 0, 0 },
        [DIRECTION_U] = { 0, 0 },
        [DIRECTION_D] = { 0, 0 },
    },
    [BLOCK_DIRT] =
    {
        [DIRECTION_E] = { 3, 0 },
        [DIRECTION_W] = { 3, 0 },
        [DIRECTION_N] = { 3, 0 },
        [DIRECTION_S] = { 3, 0 },
        [DIRECTION_U] = { 3, 0 },
        [DIRECTION_D] = { 3, 0 },
    },
    [BLOCK_GLASS] =
    {
        [DIRECTION_E] = { 0, 0 },
        [DIRECTION_W] = { 0, 0 },
        [DIRECTION_N] = { 0, 0 },
        [DIRECTION_S] = { 0, 0 },
        [DIRECTION_U] = { 0, 0 },
        [DIRECTION_D] = { 0, 0 },
    },
    [BLOCK_GRASS] =
    {
        [DIRECTION_E] = { 2, 0 },
        [DIRECTION_W] = { 2, 0 },
        [DIRECTION_N] = { 2, 0 },
        [DIRECTION_S] = { 2, 0 },
        [DIRECTION_U] = { 1, 0 },
        [DIRECTION_D] = { 3, 0 },
    },
    [BLOCK_LAVA] =
    {
        [DIRECTION_E] = { 0, 0 },
        [DIRECTION_W] = { 0, 0 },
        [DIRECTION_N] = { 0, 0 },
        [DIRECTION_S] = { 0, 0 },
        [DIRECTION_U] = { 0, 0 },
        [DIRECTION_D] = { 0, 0 },
    },
    [BLOCK_ROSE] =
    {
        [DIRECTION_E] = { 0, 2 },
        [DIRECTION_W] = { 0, 2 },
        [DIRECTION_N] = { 0, 2 },
        [DIRECTION_S] = { 0, 2 },
        [DIRECTION_U] = { 0, 2 },
        [DIRECTION_D] = { 0, 2 },
    },
    [BLOCK_SAND] =
    {
        [DIRECTION_E] = { 5, 0 },
        [DIRECTION_W] = { 5, 0 },
        [DIRECTION_N] = { 5, 0 },
        [DIRECTION_S] = { 5, 0 },
        [DIRECTION_U] = { 5, 0 },
        [DIRECTION_D] = { 5, 0 },
    },
    [BLOCK_SNOW] =
    {
        [DIRECTION_E] = { 0, 0 },
        [DIRECTION_W] = { 0, 0 },
        [DIRECTION_N] = { 0, 0 },
        [DIRECTION_S] = { 0, 0 },
        [DIRECTION_U] = { 0, 0 },
        [DIRECTION_D] = { 0, 0 },
    },
    [BLOCK_STONE] =
    {
        [DIRECTION_E] = { 4, 0 },
        [DIRECTION_W] = { 4, 0 },
        [DIRECTION_N] = { 4, 0 },
        [DIRECTION_S] = { 4, 0 },
        [DIRECTION_U] = { 4, 0 },
        [DIRECTION_D] = { 4, 0 },
    },
    [BLOCK_TULIP] =
    {
        [DIRECTION_E] = { 0, 0 },
        [DIRECTION_W] = { 0, 0 },
        [DIRECTION_N] = { 0, 0 },
        [DIRECTION_S] = { 0, 0 },
        [DIRECTION_U] = { 0, 0 },
        [DIRECTION_D] = { 0, 0 },
    },
    [BLOCK_WATER] =
    {
        [DIRECTION_E] = { 1, 1 },
        [DIRECTION_W] = { 1, 1 },
        [DIRECTION_N] = { 1, 1 },
        [DIRECTION_S] = { 1, 1 },
        [DIRECTION_U] = { 1, 1 },
        [DIRECTION_D] = { 1, 1 },
    },
};