#pragma once

#include <SDL3/SDL.h>

#include "buffer.h"
#include "chunk.h"

typedef enum worker_job_type
{
    WORKER_JOB_TYPE_NONE,
    WORKER_JOB_TYPE_QUIT,
    WORKER_JOB_TYPE_SET_BLOCKS,
    WORKER_JOB_TYPE_SET_VOXELS,
    WORKER_JOB_TYPE_SET_LIGHTS,
}
worker_job_type_t;

typedef struct worker_job
{
    worker_job_type_t type;
    int x;
    int z;
}
worker_job_t;

typedef struct worker
{
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    worker_job_t job;
    cpu_buffer_t voxels[CHUNK_MESH_TYPE_COUNT];
    cpu_buffer_t lights;
}
worker_t;

void worker_init(worker_t* worker, SDL_GPUDevice* device);
void worker_free(worker_t* worker);
void worker_dispatch(worker_t* worker, const worker_job_t* job);
void worker_wait(const worker_t* worker);
bool worker_is_working(const worker_t* worker);
