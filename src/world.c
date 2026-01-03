#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "camera.h"
#include "chunk.h"
#include "light.h"
#include "noise.h"
#include "save.h"
#include "sort.h"
#include "voxel.h"
#include "world.h"

void world_init(world_t* world, SDL_GPUDevice* device, const noise_t* noise)
{
    world->device = device;
    world->x = 0;
    world->y = 0;
    world->z = 0;
    cpu_buffer_init(&world->cpu_index_buffer, device, sizeof(uint32_t));
    gpu_buffer_init(&world->gpu_index_buffer, device, sizeof(uint32_t));
    cpu_buffer_init(&world->cpu_voxel_buffer, device, sizeof(voxel_t));
    cpu_buffer_init(&world->cpu_light_buffer, device, sizeof(light_t));
    world->noise = *noise;
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        world->sorted_chunks[x][z][0] = x;
        world->sorted_chunks[x][z][1] = z;
    }
    int w = WORLD_WIDTH;
    sort_xy(w / 2, w / 2, (int*) world->sorted_chunks, w * w);
}

void world_free(world_t* world)
{
    cpu_buffer_free(&world->cpu_index_buffer);
    gpu_buffer_free(&world->gpu_index_buffer);
    cpu_buffer_free(&world->cpu_voxel_buffer);
    cpu_buffer_free(&world->cpu_light_buffer);
}

static void move(world_t* world, const camera_t* camera)
{

}

void world_tick(world_t* world, const camera_t* camera, save_t* save)
{
    move(world, camera);
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int y = 0; y < WORLD_WIDTH; y++)
    {
        int a = world->sorted_chunks[x][y][0];
        int b = world->sorted_chunks[x][y][1];
        chunk_t* chunk = &world->chunks[a][b];
        if (chunk->flags & CHUNK_FLAG_GENERATE)
        {
            chunk_generate(chunk, &world->noise);
            // save
            // TODO: remove
            return;
        }
        if (chunk->flags & CHUNK_FLAG_MESH)
        {
            chunk_t* neighbors[3][3] = {0};
            for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                int k = a + i;
                int l = y + j;
                if (k >= 0 && l >= 0 && k < WORLD_WIDTH && l < WORLD_WIDTH)
                {
                    neighbors[i][j] = &world->chunks[k][l];
                }
            }
            chunk_mesh(chunk, neighbors, &world->cpu_voxel_buffer, &world->cpu_light_buffer, NULL);
            // TODO: remove
            return;
        }
    }
}

void world_draw(world_t* world, const camera_t* camera)
{
}