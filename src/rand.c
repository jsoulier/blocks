#include <SDL3/SDL.h>
#include <stb_perlin.h>

#include "rand.h"
#include "world.h"

void Rand_GetBlocks(void* userdata, int cx, int cz, RandSetBlock callback)
{
    for (int local_x = 0; local_x < CHUNK_WIDTH; local_x++)
    {
        for (int local_z = 0; local_z < CHUNK_WIDTH; local_z++)
        {
            int world_x = cx + local_x;
            int world_z = cz + local_z;
            bool is_lowland = false;
            bool has_grass = false;
            float terrain_height =
                stb_perlin_fbm_noise3(world_x * 0.005f, 0.0f, world_z * 0.005f, 2.0f, 0.5f, 6) *
                50.0f;
            terrain_height = SDL_powf(SDL_max(terrain_height, 0.0f), 1.3f) + 30.0f;
            terrain_height = SDL_clamp(terrain_height, 0.0f, CHUNK_HEIGHT - 1.0f);
            if (terrain_height < 40.0f)
            {
                terrain_height +=
                    stb_perlin_fbm_noise3(-world_x * 0.01f, 0.0f, world_z * 0.01f, 2.0f, 0.5f, 6) *
                    12.0f;
                is_lowland = true;
            }
            float biome =
                stb_perlin_fbm_noise3(world_x * 0.2f, 0.0f, world_z * 0.2f, 2.0f, 0.5f, 6);
            Block surface_block;
            Block fill_block;
            if (terrain_height + biome < 31.0f)
            {
                surface_block = BLOCK_SAND;
                fill_block = BLOCK_SAND;
            }
            else
            {
                biome *= 8.0f;
                biome = SDL_clamp(biome, -5.0f, 5.0f);
                if (terrain_height + biome < 61.0f)
                {
                    surface_block = BLOCK_GRASS;
                    fill_block = BLOCK_DIRT;
                    has_grass = true;
                }
                else if (terrain_height + biome < 132.0f)
                {
                    surface_block = BLOCK_STONE;
                    fill_block = BLOCK_STONE;
                }
                else
                {
                    surface_block = BLOCK_SNOW;
                    fill_block = BLOCK_STONE;
                }
            }
            int y = 0;
            for (; y < terrain_height; y++)
            {
                callback(userdata, world_x, y, world_z, fill_block);
            }
            callback(userdata, world_x, y, world_z, surface_block);
            for (; y < 30; y++)
            {
                callback(userdata, world_x, y, world_z, BLOCK_WATER);
            }
            if (is_lowland && has_grass)
            {
                float plant_noise =
                    stb_perlin_fbm_noise3(world_x * 0.2f, 0.0f, world_z * 0.2f, 2.0f, 0.5f, 3) *
                        0.5f +
                    0.5f;
                bool can_grow_tree = local_x > 2 && local_x < CHUNK_WIDTH - 2 && local_z > 2 &&
                                     local_z < CHUNK_WIDTH - 2;
                if (plant_noise > 0.8f && can_grow_tree)
                {
                    int trunk_height = 3 + plant_noise * 2.0f;
                    for (int dy = 0; dy < trunk_height; dy++)
                    {
                        callback(userdata, world_x, y + dy + 1, world_z, BLOCK_LOG);
                    }
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        for (int dz = -1; dz <= 1; dz++)
                        {
                            for (int dy = 0; dy < 2; dy++)
                            {
                                if (dx || dz || dy)
                                {
                                    callback(
                                        userdata,
                                        world_x + dx,
                                        y + trunk_height + dy,
                                        world_z + dz,
                                        BLOCK_LEAVES);
                                }
                            }
                        }
                    }
                }
                else if (plant_noise > 0.55f)
                {
                    callback(userdata, world_x, y + 1, world_z, BLOCK_BUSH);
                }
                else if (plant_noise > 0.52f)
                {
                    int flower_index = SDL_max(((int)(plant_noise * 1000.0f)) % 4, 0);
                    Block flowers[] = {BLOCK_BLUEBELL, BLOCK_GARDENIA, BLOCK_LAVENDER, BLOCK_ROSE};
                    callback(userdata, world_x, y + 1, world_z, flowers[flower_index]);
                }
            }
            if (terrain_height > 130.0f)
            {
                continue;
            }
            float cloud_noise = stb_perlin_turbulence_noise3(
                world_x * 0.015f,
                0.0f,
                world_z * 0.015f,
                2.0f,
                0.5f,
                6);
            int cloud_half_height = -1;
            if (cloud_noise > 0.9f)
            {
                cloud_half_height = 2;
            }
            else if (cloud_noise > 0.7f)
            {
                cloud_half_height = 1;
            }
            else if (cloud_noise > 0.6f)
            {
                cloud_half_height = 0;
            }
            for (int cloud_y = -cloud_half_height; cloud_y <= cloud_half_height; cloud_y++)
            {
                callback(userdata, world_x, 155 - cloud_y, world_z, BLOCK_CLOUD);
            }
        }
    }
}
