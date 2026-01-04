#pragma once

#include <SDL3/SDL.h>

#include "buffer.h"
#include "chunk.h"

typedef struct Save Save;
typedef struct World World;

typedef enum WorkerJobType
{
    WorkerJobTypeQuit,
    WorkerJobTypeGenerate,
    WorkerJobTypeMesh,
}
WorkerJobType;

typedef struct WorkerJob
{
    WorkerJobType Type;
    int X;
    int Y;
    int Z;
    World* WorldRef;
    Save* SaveRef;
    Noise* NoiseRef;
}
WorkerJob;

typedef struct Worker
{
    SDL_Thread* Thread;
    SDL_Mutex* Mutex;
    SDL_Condition* Condition;
    const WorkerJob* JobRef;
    CpuBuffer CpuVoxelBuffers[ChunkMeshTypeCount];
    CpuBuffer CpuLightBuffer;
}
Worker;

void CreateWorker(Worker* worker, SDL_GPUDevice* device);
void DestroyWorker(Worker* worker);
void DispatchWorker(Worker* worker, const WorkerJob* job);
void WaitForWorker(Worker* worker);