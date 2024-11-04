#include <stb_perlin.h>
#include <math.h>
#include <stdbool.h>
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

static void terrain(
    group_t* group,
    const int x,
    const int z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++)
    {
        const int s = x * CHUNK_X + a;
        const int t = z * CHUNK_Z + b;
        float height = stb_perlin_fbm_noise3(
            s * NOISE_FREQUENCY,
            0.0f,
            t * NOISE_FREQUENCY,
            NOISE_LACUNARITY,
            NOISE_GAIN,
            NOISE_OCTAVES);
        height *= NOISE_SCALE;
        height = powf(fmaxf(height, 0.0f), NOISE_EXPONENTIAL);
        height += NOISE_SEA;
        height = clamp(height, 0, GROUP_Y - 1);
        bool low = false;
        bool grass = false;
        if (height < NOISE_LOW)
        {
            const float f = stb_perlin_fbm_noise3(
                -s * NOISE_LOW_FREQUENCY,
                0.0f,
                t * NOISE_LOW_FREQUENCY,
                NOISE_LACUNARITY,
                NOISE_GAIN,
                NOISE_OCTAVES);
            height += f * NOISE_LOW_SCALE;
            low = true;
        }
        float biome = stb_perlin_fbm_noise3(
            s * NOISE_BIOME_FREQUENCY,
            0.0f,
            t * NOISE_BIOME_FREQUENCY,
            NOISE_LACUNARITY,
            NOISE_GAIN,
            NOISE_OCTAVES);
        block_t top;
        block_t bottom;
        if (height + biome < NOISE_GRASS)
        {
            top = BLOCK_SAND;
            bottom = BLOCK_SAND;
        }
        else
        {
            biome *= NOISE_BIOME_SCALE;
            biome = clamp(biome, -NOISE_BIOME_CLAMP, NOISE_BIOME_CLAMP);
            if (height + biome < NOISE_MOUNTAIN)
            {
                top = BLOCK_GRASS;
                bottom = BLOCK_DIRT;
                grass = true;
            }
            else if (height + biome < NOISE_SNOW)
            {
                top = BLOCK_STONE;
                bottom = BLOCK_STONE;
            }
            else
            {
                top = BLOCK_SNOW;
                bottom = BLOCK_STONE;
            }
        }
        int y = 0;
        for (; y < height; y++)
        {
            group_set_block(group, a, y, b, bottom);
        }
        group_set_block(group, a, y, b, top);
        if (low && grass)
        {
            // TODO:
        }
        for (; y < NOISE_SEA; y++)
        {
            group_set_block(group, a, y, b, BLOCK_WATER);
        }
        const float clouds = stb_perlin_turbulence_noise3(
            s * NOISE_CLOUD_FREQUENCY,
            NOISE_CLOUD_Y * NOISE_CLOUD_FREQUENCY,
            t * NOISE_CLOUD_FREQUENCY,
            NOISE_LACUNARITY,
            NOISE_GAIN,
            NOISE_OCTAVES);
        if (clouds > NOISE_CLOUD_THRESHOLD && NOISE_CLOUD_Y > clouds + NOISE_CLOUD_CLEARANCE)
        {
            for (int y = 0; y < clouds * NOISE_CLOUD_THICKNESS; y++)
            {
                group_set_block(group, a, NOISE_CLOUD_Y + y, b, BLOCK_CLOUD);
            }
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
        terrain(group, x, z);
        break;
    default:
        assert(0);
    }
}