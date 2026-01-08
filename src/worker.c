#include <SDL3/SDL.h>

#include "chunk.h"
#include "light.h"
#include "save.h"
#include "voxel.h"
#include "worker.h"
#include "world.h"

static int worker_func(void* args)
{
    worker_t* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->mutex);
        while (!worker->job)
        {
            SDL_WaitCondition(worker->condition, worker->mutex);
        }
        if (worker->job->type == WORKER_JOB_TYPE_QUIT)
        {
            worker->job = NULL;
            SDL_SignalCondition(worker->condition);
            SDL_UnlockMutex(worker->mutex);
            return 0;
        }
        const worker_job_t* job = worker->job;
        chunk_t* chunk = world_get_chunk(job->x, job->z);
        SDL_assert(chunk);
        if (job->type == WORKER_JOB_TYPE_SET_BLOCKS)
        {
            chunk_set_blocks(chunk);
            save_get_chunk(chunk);
        }
        else
        {
            chunk_t* chunks[3][3];
            world_get_chunks(job->x, job->z, chunks);
            if (job->type == WORKER_JOB_TYPE_SET_VOXELS)
            {
                chunk_set_voxels(chunks, worker->voxels);
            }
            else if (job->type == WORKER_JOB_TYPE_SET_LIGHTS)
            {
                chunk_set_lights(chunks, &worker->lights);
            }
            else
            {
                SDL_assert(false);
            }
        }
        worker->job = NULL;
        SDL_SignalCondition(worker->condition);
        SDL_UnlockMutex(worker->mutex);
    }
    return 0;
}

void worker_init(worker_t* worker, SDL_GPUDevice* device)
{
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_init(&worker->voxels[i], device, sizeof(voxel_t));
    }
    cpu_buffer_init(&worker->lights, device, sizeof(light_t));
    worker->mutex = SDL_CreateMutex();
    if (!worker->mutex)
    {
        SDL_Log("Failed to create mutex: %s", SDL_GetError());
    }
    worker->condition = SDL_CreateCondition();
    if (!worker->condition)
    {
        SDL_Log("Failed to create condition variable: %s", SDL_GetError());
    }
    worker->thread = SDL_CreateThread(worker_func, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

void worker_free(worker_t* worker)
{
    worker_job_t job = {0};
    job.type = WORKER_JOB_TYPE_QUIT;
    worker_dispatch(worker, &job);
    SDL_WaitThread(worker->thread, NULL);
    SDL_DestroyMutex(worker->mutex);
    SDL_DestroyCondition(worker->condition);
    worker->thread = NULL;
    worker->mutex = NULL;
    worker->condition = NULL;
    for (int i = 0; i < CHUNK_MESH_TYPE_COUNT; i++)
    {
        cpu_buffer_free(&worker->voxels[i]);
    }
    cpu_buffer_free(&worker->lights);
}

void worker_dispatch(worker_t* worker, const worker_job_t* job)
{
    SDL_LockMutex(worker->mutex);
    SDL_assert(!worker->job);
    worker->job = job;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}

void worker_wait(const worker_t* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->job)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    SDL_UnlockMutex(worker->mutex);
}
