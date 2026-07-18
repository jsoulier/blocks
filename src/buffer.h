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
}
CPUBuffer;

void CPUBuffer_Init(CPUBuffer* cpu, SDL_GPUDevice* device, Uint32 stride);
void CPUBuffer_Free(CPUBuffer* cpu);
void CPUBuffer_Append(CPUBuffer* cpu, void* item);

typedef struct GPUBuffer
{
    SDL_GPUDevice* device;
    SDL_GPUBufferUsageFlags usage;
    SDL_GPUBuffer* buffer;
    Uint32 size;
    Uint32 capacity;
}
GPUBuffer;

void GPUBuffer_Init(GPUBuffer* gpu, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage);
void GPUBuffer_Free(GPUBuffer* gpu);
void GPUBuffer_Upload(GPUBuffer* gpu, CPUBuffer* cpu);
void GPUBuffer_Clear(GPUBuffer* gpu);
bool GPUBuffer_BeginUpload(GPUBuffer* gpu);
void GPUBuffer_EndUpload(GPUBuffer* gpu);
