#include <SDL3/SDL.h>

#include "worker.h"

static int Run(void* args)
{
    Worker* worker = args;
    while (true)
    {
        SDL_LockMutex(worker->mutex);
        while (!worker->task && !worker->quit)
        {
            SDL_WaitCondition(worker->condition, worker->mutex);
        }
        if (worker->quit)
        {
            SDL_UnlockMutex(worker->mutex);
            return 0;
        }
        WorkerTask task = worker->task;
        void* data = worker->data;
        SDL_UnlockMutex(worker->mutex);
        task(data);
        SDL_LockMutex(worker->mutex);
        worker->task = NULL;
        worker->data = NULL;
        SDL_SignalCondition(worker->condition);
        SDL_UnlockMutex(worker->mutex);
    }
}

void Worker_Init(Worker* worker)
{
    worker->task = NULL;
    worker->data = NULL;
    worker->quit = false;
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
    worker->thread = SDL_CreateThread(Run, "worker", worker);
    if (!worker->thread)
    {
        SDL_Log("Failed to create thread: %s", SDL_GetError());
    }
}

void Worker_Free(Worker* worker)
{
    SDL_LockMutex(worker->mutex);
    while (worker->task)
    {
        SDL_WaitCondition(worker->condition, worker->mutex);
    }
    worker->quit = true;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
    SDL_WaitThread(worker->thread, NULL);
    SDL_DestroyMutex(worker->mutex);
    SDL_DestroyCondition(worker->condition);
    worker->thread = NULL;
    worker->mutex = NULL;
    worker->condition = NULL;
}

bool Worker_IsBusy(Worker* worker)
{
    SDL_LockMutex(worker->mutex);
    bool busy = worker->task != NULL;
    SDL_UnlockMutex(worker->mutex);
    return busy;
}

void Worker_Dispatch(Worker* worker, WorkerTask task, void* data)
{
    SDL_assert(!Worker_IsBusy(worker));
    SDL_LockMutex(worker->mutex);
    SDL_assert(!worker->task);
    SDL_assert(!worker->quit);
    worker->task = task;
    worker->data = data;
    SDL_SignalCondition(worker->condition);
    SDL_UnlockMutex(worker->mutex);
}
