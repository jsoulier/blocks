#include <SDL3/SDL.h>
#include <stb_perlin.h>

#include "rand.h"
#include "world.h"

void Rand_GetBlocks(void* userdata, int cx, int cz, RandSetBlock callback)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    for (int z = 0; z < CHUNK_WIDTH; z++)
    {
        int bx = cx + x;
        int bz = cz + z;
        float height = stb_perlin_fbm_noise3(bx * 0.005f, 0.0f, bz * 0.005f, 2.0f, 0.5f, 6) * 50.0f;
        height = SDL_powf(SDL_max(height, 0.0f), 1.3f) + 30.0f;
        height = SDL_clamp(height, 0.0f, CHUNK_HEIGHT - 1.0f);
        bool is_low_elevation = false;
        if (height < 40.0f)
        {
            height += stb_perlin_fbm_noise3(-bx * 0.01f, 0.0f, bz * 0.01f, 2.0f, 0.5f, 6) * 12.0f;
            is_low_elevation = true;
        }
        float biome = stb_perlin_fbm_noise3(bx * 0.2f, 0.0f, bz * 0.2f, 2.0f, 0.5f, 6);
        Block top;
        Block bottom;
        if (height + biome < 31.0f)
        {
            top = BLOCK_SAND;
            bottom = BLOCK_SAND;
        }
        else
        {
            biome *= 8.0f;
            biome = SDL_clamp(biome, -5.0f, 5.0f);
            if (height + biome < 61.0f)
            {
                top = BLOCK_GRASS;
                bottom = BLOCK_DIRT;
            }
            else if (height + biome < 132.0f)
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
            callback(userdata, bx, y, bz, bottom);
        }
        callback(userdata, bx, y, bz, top);
        for (; y < 30; y++)
        {
            callback(userdata, bx, y, bz, BLOCK_WATER);
        }
        if (top == BLOCK_GRASS && is_low_elevation)
        {
            float plant = stb_perlin_fbm_noise3(bx * 0.2f, 0.0f, bz * 0.2f, 2.0f, 0.5f, 3) * 0.5f + 0.5f;
            if (plant > 0.8f && x > 2 && x < CHUNK_WIDTH - 2 && z > 2 && z < CHUNK_WIDTH - 2)
            {
                int trunk = 3 + plant * 2.0f;
                for (int dy = 0; dy < trunk; dy++)
                {
                    callback(userdata, bx, y + dy + 1, bz, BLOCK_LOG);
                }
                for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                for (int dy = 0; dy < 2; dy++)
                {
                    if (dx || dz || dy)
                    {
                        callback(userdata, bx + dx, y + trunk + dy, bz + dz, BLOCK_LEAVES);
                    }
                }
            }
            else if (plant > 0.55f)
            {
                callback(userdata, bx, y + 1, bz, BLOCK_BUSH);
            }
            else if (plant > 0.52f)
            {
                int i = (int)(plant * 1000.0f) % 4;
                Block flowers[] = {BLOCK_BLUEBELL, BLOCK_GARDENIA, BLOCK_LAVENDER, BLOCK_ROSE};
                callback(userdata, bx, y + 1, bz, flowers[i]);
            }
        }
        if (height > 130.0f)
        {
            continue;
        }
        float cloud = stb_perlin_turbulence_noise3(bx * 0.015f, 0.0f, bz * 0.015f, 2.0f, 0.5f, 6);
        int scale = -1;
        if (cloud > 0.9f)
        {
            scale = 2;
        }
        else if (cloud > 0.7f)
        {
            scale = 1;
        }
        else if (cloud > 0.6f)
        {
            scale = 0;
        }
        for (int y = -scale; y <= scale; y++)
        {
            callback(userdata, bx, 155 - y, bz, BLOCK_CLOUD);
        }
    }
}
