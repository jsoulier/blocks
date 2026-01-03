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

void CreateWorld(World* world, SDL_GPUDevice* device)
{
    world->Device = device;
    world->X = 0;
    world->Y = 0;
    world->Z = 0;
    CreateCpuBuffer(&world->CpuIndexBuffer, device, sizeof(uint32_t));
    CreateGpuBuffer(&world->GpuIndexBuffer, device, sizeof(uint32_t));
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        CreateCpuBuffer(&world->CpuVoxelBuffers[i], device, sizeof(Voxel));
    }
    CreateCpuBuffer(&world->CpuLightBuffer, device, sizeof(Light));
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int z = 0; z < WORLD_WIDTH; z++)
    {
        world->SortedChunks[x][z][0] = x;
        world->SortedChunks[x][z][1] = z;
    }
    int w = WORLD_WIDTH;
    SortXY(w / 2, w / 2, (int*) world->SortedChunks, w * w);
}

void DestroyWorld(World* world)
{
    DestroyCpuBuffer(&world->CpuIndexBuffer);
    DestroyGpuBuffer(&world->GpuIndexBuffer);
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        DestroyCpuBuffer(&world->CpuVoxelBuffers[i]);
    }
    DestroyCpuBuffer(&world->CpuLightBuffer);
}

static void Move(World* world, const Camera* camera)
{

}

void UpdateWorld(World* world, const Camera* camera, Save* save, Noise* noise)
{
    Move(world, camera);
    for (int x = 0; x < WORLD_WIDTH; x++)
    for (int y = 0; y < WORLD_WIDTH; y++)
    {
        int a = world->SortedChunks[x][y][0];
        int b = world->SortedChunks[x][y][1];
        Chunk* chunk = &world->Chunks[a][b];
        if (chunk->Flags & ChunkFlagGenerate)
        {
            GenerateChunk(chunk, noise);
            // save
            return; // TODO: remove 
        }
        if (chunk->Flags & ChunkFlagMesh)
        {
            Chunk* neighbors[3][3] = {0};
            for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                int k = a + i;
                int l = y + j;
                if (k >= 0 && l >= 0 && k < WORLD_WIDTH && l < WORLD_WIDTH)
                {
                    neighbors[i][j] = &world->Chunks[k][l];
                }
            }
            MeshChunk(chunk, neighbors, NULL, world->CpuVoxelBuffers, &world->CpuLightBuffer);
            return; // TODO: remove 
        }
    }
}

void DrawWorld(World* world, const Camera* camera)
{
}