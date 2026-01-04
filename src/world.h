#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "chunk.h"
#include "worker.h"

#define WORLD_WIDTH 4
#define WORLD_WORKERS 1

typedef struct Camera Camera;
typedef struct Noise Noise;
typedef struct Save Save;

typedef struct World
{
    SDL_GPUDevice* Device;
    int X;
    int Y;
    int Z;
    Worker Workers[WORLD_WORKERS];
    CpuBuffer CpuIndexBuffer;
    GpuBuffer GpuIndexBuffer;
    Chunk* Chunks[WORLD_WIDTH][WORLD_WIDTH];
    int SortedChunks[WORLD_WIDTH - 2][WORLD_WIDTH - 2][2];
}
World;

void CreateWorld(World* world, SDL_GPUDevice* device);
void DestroyWorld(World* world);
void UpdateWorld(World* world, const Camera* camera, Save* save, Noise* noise);
void RenderWorld(World* world, const Camera* camera, SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* pass, ChunkMeshType type);
Chunk* GetWorldChunk(World* world, int x, int y, int z);