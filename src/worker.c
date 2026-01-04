#include <SDL3/SDL.h>

#include "chunk.h"
#include "light.h"
#include "save.h"
#include "voxel.h"
#include "worker.h"
#include "world.h"

static int Loop(void* args)
{
    Worker* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->Mutex);
        while (!worker->JobRef)
        {
            SDL_WaitCondition(worker->Condition, worker->Mutex);
        }
        if (worker->JobRef->Type == WorkerJobTypeQuit)
        {
            worker->JobRef = NULL;
            SDL_SignalCondition(worker->Condition);
            SDL_UnlockMutex(worker->Mutex);
            return 0;
        }
        const WorkerJob* job = worker->JobRef;
        Chunk* chunk = GetWorldChunk(job->WorldRef, job->X, job->Y, job->Z);
        SDL_assert(chunk);
        switch (job->Type)
        {
        case WorkerJobTypeGenerate:
            GenerateChunk(chunk, job->NoiseRef);
            // TODO: use save
            break;
        case WorkerJobTypeMesh:
            Chunk* neighbors[3][3] = {0};
            for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++)
            {
                int k = job->X + i;
                int l = job->Z + j;
                neighbors[i + 1][j + 1] = GetWorldChunk(job->WorldRef, k, job->Y, l);
            }
            MeshChunk(chunk, neighbors, worker->CpuVoxelBuffers, &worker->CpuLightBuffer);
            break;
        default:
            SDL_assert(false);
        }
        worker->JobRef = NULL;
        SDL_SignalCondition(worker->Condition);
        SDL_UnlockMutex(worker->Mutex);
    }
    return 0;
}

void CreateWorker(Worker* worker, SDL_GPUDevice* device)
{
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        CreateCpuBuffer(&worker->CpuVoxelBuffers[i], device, sizeof(Voxel));
    }
    CreateCpuBuffer(&worker->CpuLightBuffer, device, sizeof(Light));
    worker->Mutex = SDL_CreateMutex();
    if (!worker->Mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    worker->Condition = SDL_CreateCondition();
    if (!worker->Condition)
    {
        SDL_Log("Failed to create condition variable: %s", SDL_GetError());
    }
    worker->Thread = SDL_CreateThread(Loop, "worker", worker);
    if (!worker->Thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

void DestroyWorker(Worker* worker)
{
    WorkerJob job;
    job.Type = WorkerJobTypeQuit;
    DispatchWorker(worker, &job);
    SDL_WaitThread(worker->Thread, NULL);
    SDL_DestroyMutex(worker->Mutex);
    SDL_DestroyCondition(worker->Condition);
    worker->Thread = NULL;
    worker->Mutex = NULL;
    worker->Condition = NULL;
    for (int i = 0; i < ChunkMeshTypeCount; i++)
    {
        DestroyCpuBuffer(&worker->CpuVoxelBuffers[i]);
    }
    DestroyCpuBuffer(&worker->CpuLightBuffer);
}

void DispatchWorker(Worker* worker, const WorkerJob* job)
{
    SDL_LockMutex(worker->Mutex);
    SDL_assert(!worker->JobRef);
    worker->JobRef = job;
    SDL_SignalCondition(worker->Condition);
    SDL_UnlockMutex(worker->Mutex);
}

void WaitForWorker(Worker* worker)
{
    SDL_LockMutex(worker->Mutex);
    while (worker->JobRef)
    {
        SDL_WaitCondition(worker->Condition, worker->Mutex);
    }
    SDL_UnlockMutex(worker->Mutex);
}
