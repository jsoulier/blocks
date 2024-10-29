#include <stdint.h>
#include "helpers.h"
#include "noise.h"
#include "world.h"

static void (*generator)(
    group_t* group,
    const int32_t x,
    const int32_t y);

void set(
    group_t* group,
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    assert(group);
    const int a = y / CHUNK_Y;
    const int b = y - a * CHUNK_Y;
    chunk_t* chunk = &group->chunks[a];
    chunk->blocks[x][b][z] = block;
    chunk->empty = false;
}

static void cube(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    for (int a = 1; a < CHUNK_X - 1; a++)
    for (int b = 1; b < CHUNK_Z - 1; b++)
    for (int y = 1; y < CHUNK_Y - 1; y++)
    {
        if (y >= CHUNK_Y - 2)
        {
            set(group, a, y, b, BLOCK_GRASS);
        }
        else if (y >= CHUNK_Y - 5)
        {
            set(group, a, y, b, BLOCK_DIRT);
        }
        else
        {
            set(group, a, y, b, BLOCK_STONE);
        }
    }
}

static void flat(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++)
    {
        set(group, a, 0, b, BLOCK_STONE);
        set(group, a, 1, b, BLOCK_DIRT);
        set(group, a, 2, b, BLOCK_GRASS);
    }
}

void noise_init(const noise_t noise)
{
    switch (noise)
    {
    case NOISE_CUBE:
        generator = cube;
        break;
    case NOISE_FLAT:
        generator = flat;
        break;
    default:
        assert(0);
    }
}

void noise_generate(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    assert(generator);
    generator(group, x, z);
}