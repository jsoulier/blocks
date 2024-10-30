#include <stb_perlin.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "block.h"
#include "containers.h"
#include "helpers.h"
#include "noise.h"
#include "world.h"

static noise_type_t type;
static int seed;

static void cube(
    group_t* group,
    const int x,
    const int z)
{
    for (int a = 1; a < CHUNK_X - 1; a++)
    for (int b = 1; b < CHUNK_Z - 1; b++)
    for (int y = 1; y < CHUNK_Y - 1; y++)
    {
        if (y >= CHUNK_Y - 2)
        {
            group_set_block(group, a, y, b, BLOCK_GRASS);
        }
        else if (y >= CHUNK_Y - 5)
        {
            group_set_block(group, a, y, b, BLOCK_DIRT);
        }
        else
        {
            group_set_block(group, a, y, b, BLOCK_STONE);
        }
    }
}

static void flat(
    group_t* group,
    const int x,
    const int z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++)
    {
        group_set_block(group, a, 0, b, BLOCK_STONE);
        group_set_block(group, a, 1, b, BLOCK_DIRT);
        group_set_block(group, a, 2, b, BLOCK_GRASS);
    }
}

static float base(
    const int x,
    const int z,
    const int a,
    const int b)
{
    const int c = x * CHUNK_X + a;
    const int d = z * CHUNK_Z + b;
    const float f = 0.005f;
    const float amplitude = 50.0f;
    const float h = stb_perlin_noise3_seed(c * f, 0.0f, d * f, 0, 0, 0, seed);
    return abs(h) * amplitude;
}

static void terrain_v1(
    group_t* group,
    const int x,
    const int z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++)
    {
        float height = base(x, z, a, b) + 10;
        for (int h = 0; h <= height; h++)
        {
            block_t block;
            if (h < 10)
            {
                block = BLOCK_SAND;
            }
            else
            {
                block = BLOCK_GRASS;
            }
            group_set_block(group, a, h, b, block);
        }
        const int sea = 15;
        for (int h = height; h < sea; h++)
        {
            group_set_block(group, a, h, b, BLOCK_WATER);
        }
    }
}

void noise_init(
    const noise_type_t t,
    const int s)
{
    type = t;
    if (!s)
    {
        srand(time(NULL));
        seed = rand() % 64;
        if (!seed)
        {
            seed = 1;
        }
    }
    else
    {
        seed = s;
    }
}

void noise_data(
    noise_type_t* t,
    int* s)
{
    assert(t);
    assert(s);
    *t = type;
    *s = seed;
}

void noise_generate(
    group_t* group,
    const int x,
    const int z)
{
    assert(group);
    switch (type)
    {
    case NOISE_TYPE_CUBE:
        cube(group, x, z);
        break;
    case NOISE_TYPE_FLAT:
        flat(group, x, z);
        break;
    case NOISE_TYPE_TERRAIN_V1:
        terrain_v1(group, x, z);
        break;
    default:
        assert(0);
    }
}