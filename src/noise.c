#include <stb_perlin.h>
#include "block.h"
#include "containers.h"
#include "helpers.h"
#include "noise.h"

void noise_generate(
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
            const float plant = stb_perlin_fbm_noise3(
                s * NOISE_PLANT_FREQUENCY,
                0.0f,
                t * NOISE_PLANT_FREQUENCY,
                NOISE_LACUNARITY,
                NOISE_GAIN,
                NOISE_OCTAVES);
            if (plant > NOISE_TREE_THRESHOLD &&
                a > 2 && a < CHUNK_X - 2 &&
                b > 2 && b < CHUNK_Z - 2)
            {
                const int log = 3 + plant * 2.0f;
                for (int dy = 0; dy < log; dy++)
                {
                    group_set_block(group, a, y + dy + 1, b, BLOCK_LOG);
                }
                for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                for (int dy = 0; dy < 2; dy++)
                {
                    if (dx != 0 || dz != 0 || dy != 0)
                    {
                        group_set_block(group, a + dx, y + log + dy, b + dz, BLOCK_LEAVES);
                    }
                }
            }
            else if (plant > NOISE_FLOWER_THRESHOLD)
            {
                if (plant > 0.5)
                {
                    group_set_block(group, a, y + 1, b, BLOCK_ROSE);
                }
                else
                {
                    group_set_block(group, a, y + 1, b, BLOCK_DANDELION);
                }
            }
        }
        for (; y < NOISE_SEA; y++)
        {
            group_set_block(group, a, y, b, BLOCK_WATER);
        }
        const float cloud = stb_perlin_turbulence_noise3(
            s * NOISE_CLOUD_FREQUENCY,
            NOISE_CLOUD_Y * NOISE_CLOUD_FREQUENCY,
            t * NOISE_CLOUD_FREQUENCY,
            NOISE_LACUNARITY,
            NOISE_GAIN,
            NOISE_OCTAVES);
        if (cloud > NOISE_CLOUD_THRESHOLD && NOISE_CLOUD_Y > height + NOISE_CLOUD_CLEARANCE)
        {
            for (int y = 0; y < cloud * NOISE_CLOUD_THICKNESS; y++)
            {
                group_set_block(group, a, NOISE_CLOUD_Y + y, b, BLOCK_CLOUD);
            }
        }
    }
}