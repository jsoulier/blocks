#pragma once

#include <SDL3/SDL.h>

typedef void (*WorkerTask)(void* data);

typedef struct Worker
{
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    SDL_Condition* condition;
    WorkerTask task;
    void* data;
    bool quit;
} Worker;

void Worker_Init(Worker* worker);
void Worker_Free(Worker* worker);
bool Worker_IsBusy(Worker* worker);
void Worker_Dispatch(Worker* worker, WorkerTask task, void* data);
