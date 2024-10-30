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
    for (int y = 1; y < CHUNK_Y - 1; y++) {
        if (y >= CHUNK_Y - 2) {
            set(group, a, y, b, BLOCK_GRASS);
        } else if (y >= CHUNK_Y - 5) {
            set(group, a, y, b, BLOCK_DIRT);
        } else {
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
    for (int b = 0; b < CHUNK_Z; b++) {
        set(group, a, 0, b, BLOCK_STONE);
        set(group, a, 1, b, BLOCK_DIRT);
        set(group, a, 2, b, BLOCK_GRASS);
    }
}

static float base(
    const int32_t x,
    const int32_t z,
    const int32_t a,
    const int32_t b)
{
    const int32_t c = x * CHUNK_X + a;
    const int32_t d = z * CHUNK_Z + b;
    const float f = 0.005f;
    const float amplitude = 50.0f;
    const float h = stb_perlin_noise3_seed(c * f, 0.0f, d * f, 0, 0, 0, seed);
    return abs(h) * amplitude;
}

static void v1(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++) {
        float height = base(x, z, a, b);
        for (int h = 0; h <= height; h++) {
            block_t block;
            if (h < 10) {
                block = BLOCK_SAND;
            } else if (h > 60.0f) {
                block = BLOCK_STONE;
            } else if (h > 40.0f) {
                block = BLOCK_DIRT;
            } else {
                block = BLOCK_GRASS;
            }
            set(group, a, h, b, block);
        }
    }
}

void noise_init(
    const noise_type_t t,
    const int s)
{
    type = t;
    if (!s) {
        srand(time(NULL));
        seed = rand() % 64;
        if (!seed) {
            seed = 1;
        }
    } else {
        seed = s;
    }
}

noise_type_t noise_type()
{
    return type;
}

int noise_seed()
{
    return seed;
}

void noise_generate(
    group_t* group,
    const int32_t x,
    const int32_t z)
{
    assert(group);
    switch (type) {
    case NOISE_TYPE_CUBE:
        cube(group, x, z);
        break;
    case NOISE_TYPE_FLAT:
        flat(group, x, z);
        break;
    case NOISE_TYPE_V1:
        v1(group, x, z);
        break;
    default:
        assert(0);
    }
}