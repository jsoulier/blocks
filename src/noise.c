#include <SDL3/SDL.h>
#include <stb_perlin.h>
#include "block.h"
#include "chunk.h"
#include "helpers.h"
#include "noise.h"

void noise_generate(
    chunk_t* chunk,
    const int x,
    const int z)
{
    for (int a = 0; a < CHUNK_X; a++)
    for (int b = 0; b < CHUNK_Z; b++)
    {
        const int s = x * CHUNK_X + a;
        const int t = z * CHUNK_Z + b;
        bool low = false;
        bool grass = false;
        float height = stb_perlin_fbm_noise3(s * 0.005f, 0.0f, t * 0.005f, 2.0f, 0.5f, 6);
        height *= 50.0f;
        height = SDL_powf(max(height, 0.0f), 1.3f);
        height += 30;
        height = clamp(height, 0, CHUNK_Y - 1);
        if (height < 40)
        {
            height += stb_perlin_fbm_noise3(-s * 0.01f, 0.0f, t * 0.01f, 2.0f, 0.5f, 6) * 12.0f;
            low = true;
        }
        float biome = stb_perlin_fbm_noise3(s * 0.2f, 0.0f, t * 0.2f, 2.0f, 0.5f, 6);
        block_t top;
        block_t bottom;
        if (height + biome < 31)
        {
            top = BLOCK_SAND;
            bottom = BLOCK_SAND;
        }
        else
        {
            biome *= 8.0f;
            biome = clamp(biome, -5.0f, 5.0f);
            if (height + biome < 61)
            {
                top = BLOCK_GRASS;
                bottom = BLOCK_DIRT;
                grass = true;
            }
            else if (height + biome < 132)
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
            chunk_set_block(chunk, a, y, b, bottom);
        }
        chunk_set_block(chunk, a, y, b, top);
        for (; y < 30; y++)
        {
            chunk_set_block(chunk, a, y, b, BLOCK_WATER);
        }
        if (low && grass)
        {
            const float plant = stb_perlin_fbm_noise3(s * 0.2f, 0.0f, t * 0.2f, 2.0f, 0.5f, 3) * 0.5f + 0.5f;
            if (plant > 0.8f && a > 2 && a < CHUNK_X - 2 && b > 2 && b < CHUNK_Z - 2)
            {
                const int log = 3 + plant * 2.0f;
                for (int dy = 0; dy < log; dy++)
                {
                    chunk_set_block(chunk, a, y + dy + 1, b, BLOCK_LOG);
                }
                for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                for (int dy = 0; dy < 2; dy++)
                {
                    if (dx || dz || dy)
                    {
                        chunk_set_block(chunk, a + dx, y + log + dy, b + dz, BLOCK_LEAVES);
                    }
                }
            }
            else if (plant > 0.55f)
            {
                chunk_set_block(chunk, a, y + 1, b, BLOCK_BUSH);
            }
            else if (plant > 0.52f)
            {
                const int value = max(((int) (plant * 1000.0f)) % 4, 0);
                const block_t flowers[] = {BLOCK_BLUEBELL, BLOCK_DANDELION, BLOCK_LAVENDER, BLOCK_ROSE};
                chunk_set_block(chunk, a, y + 1, b, flowers[value]);
            }
        }
        if (height > 130)
        {
            continue;
        }
        const float cloud = stb_perlin_turbulence_noise3(s * 0.015f, 0.0f, t * 0.015f, 2.0f, 0.5f, 6);
        int scale = -1;
        if (cloud > 0.9f)
        {
            scale = 2;
        }
        else if (cloud > 0.7f)
        {
            scale = 1;
        }
        else if (cloud > 0.6)
        {
            scale = 0;
        }
        for (int y = -scale; y <= scale; y++)
        {
            chunk_set_block(chunk, a, 155 - y, b, BLOCK_CLOUD);
        }
    }
}