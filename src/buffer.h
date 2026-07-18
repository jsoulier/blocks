#pragma once

#include <SDL3/SDL.h>

typedef struct CPUBuffer
{
    SDL_GPUDevice* device;
    SDL_GPUTransferBuffer* buffer;
    Uint8* data;
    Uint32 size;
    Uint32 capacity;
    Uint32 stride;
} CPUBuffer;

void CPUBuffer_Init(CPUBuffer* buffer, SDL_GPUDevice* device, Uint32 stride);
void CPUBuffer_Free(CPUBuffer* buffer);
void CPUBuffer_Append(CPUBuffer* buffer, const void* item);

typedef struct GPUBuffer
{
    SDL_GPUDevice* device;
    SDL_GPUBufferUsageFlags usage;
    SDL_GPUBuffer* buffer;
    Uint32 size;
    Uint32 capacity;
} GPUBuffer;

void GPUBuffer_Init(GPUBuffer* buffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage);
void GPUBuffer_Free(GPUBuffer* buffer);
void GPUBuffer_Upload(GPUBuffer* destination, CPUBuffer* source);
void GPUBuffer_Clear(GPUBuffer* buffer);
bool GPUBuffer_BeginUpload(GPUBuffer* buffer);
void GPUBuffer_EndUpload();
