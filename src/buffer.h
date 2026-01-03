#pragma once

#include <SDL3/SDL.h>

typedef struct CpuBuffer
{
    SDL_GPUDevice* Device;
    SDL_GPUTransferBuffer* Buffer;
    Uint8* Data;
    Uint32 Size;
    Uint32 Capacity;
    Uint32 Stride;
}
CpuBuffer;

void CreateCpuBuffer(CpuBuffer* cpuBuffer, SDL_GPUDevice* device, Uint32 stride);
void DestroyCpuBuffer(CpuBuffer* cpuBuffer);
void AppendCpuBuffer(CpuBuffer* cpuBuffer, void* item);

typedef struct GpuBuffer
{
    SDL_GPUDevice* Device;
    SDL_GPUBufferUsageFlags Usage;
    SDL_GPUBuffer* Buffer;
    Uint32 Size;
    Uint32 Capacity;
}
GpuBuffer;

void CreateGpuBuffer(GpuBuffer* gpuBuffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage);
void DestroyGpuBuffer(GpuBuffer* gpuBuffer);
void UpdateGpuBuffer(GpuBuffer* gpuBuffer, SDL_GPUCopyPass* pass, CpuBuffer* cpuBuffer);
