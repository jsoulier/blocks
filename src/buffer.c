#include <SDL3/SDL.h>

#include "buffer.h"

void CreateCpuBuffer(CpuBuffer* cpuBuffer, SDL_GPUDevice* device, Uint32 stride)
{
    SDL_assert(stride);
    cpuBuffer->Device = device;
    cpuBuffer->Buffer = NULL;
    cpuBuffer->Data = NULL;
    cpuBuffer->Capacity = 0;
    cpuBuffer->Size = 0;
    cpuBuffer->Stride = stride;
}

void DestroyCpuBuffer(CpuBuffer* cpuBuffer)
{
    SDL_ReleaseGPUTransferBuffer(cpuBuffer->Device, cpuBuffer->Buffer);
    cpuBuffer->Device = NULL;
    cpuBuffer->Buffer = NULL;
    cpuBuffer->Data = NULL;
    cpuBuffer->Capacity = 0;
    cpuBuffer->Size = 0;
    cpuBuffer->Stride = 0;
}

void AppendCpuBuffer(CpuBuffer* cpuBuffer, void* item)
{
    Uint32 stride = cpuBuffer->Stride;
    if (!cpuBuffer->Data && cpuBuffer->Buffer)
    {
        SDL_assert(!cpuBuffer->Size);
        cpuBuffer->Data = SDL_MapGPUTransferBuffer(cpuBuffer->Device, cpuBuffer->Buffer, true);
        if (!cpuBuffer->Data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            return;
        }
    }
    SDL_assert(cpuBuffer->Size <= cpuBuffer->Capacity);
    if (cpuBuffer->Size == cpuBuffer->Capacity)
    {
        int capacity = SDL_max(64, cpuBuffer->Size * 2);
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = capacity * stride;
        SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(cpuBuffer->Device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return;
        }
        void* data = SDL_MapGPUTransferBuffer(cpuBuffer->Device, buffer, false);
        if (!data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(cpuBuffer->Device, buffer);
            return;
        }
        if (cpuBuffer->Data)
        {
            SDL_memcpy(data, cpuBuffer->Data, cpuBuffer->Size * stride);
            SDL_UnmapGPUTransferBuffer(cpuBuffer->Device, cpuBuffer->Buffer);
        }
        SDL_ReleaseGPUTransferBuffer(cpuBuffer->Device, cpuBuffer->Buffer);
        cpuBuffer->Capacity = capacity;
        cpuBuffer->Buffer = buffer;
        cpuBuffer->Data = data;
    }
    SDL_assert(cpuBuffer->Data);
    SDL_memcpy(cpuBuffer->Data + cpuBuffer->Size * stride, item, stride);
    cpuBuffer->Size++;
}

void CreateGpuBuffer(GpuBuffer* gpuBuffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage)
{
    gpuBuffer->Device = device;
    gpuBuffer->Usage = usage;
    gpuBuffer->Buffer = NULL;
    gpuBuffer->Capacity = 0;
    gpuBuffer->Size = 0;
}

void DestroyGpuBuffer(GpuBuffer* gpuBuffer)
{
    SDL_ReleaseGPUBuffer(gpuBuffer->Device, gpuBuffer->Buffer);
    gpuBuffer->Buffer = NULL;
    gpuBuffer->Capacity = 0;
    gpuBuffer->Size = 0;
}

void UpdateGpuBuffer(GpuBuffer* gpuBuffer, SDL_GPUCopyPass* pass, CpuBuffer* cpuBuffer)
{
    gpuBuffer->Size = 0;
    if (cpuBuffer->Data)
    {
        SDL_UnmapGPUTransferBuffer(gpuBuffer->Device, cpuBuffer->Buffer);
        cpuBuffer->Data = NULL;
    }
    if (!cpuBuffer->Size)
    {
        gpuBuffer->Size = 0;
        return;
    }
    Uint32 size = cpuBuffer->Size;
    cpuBuffer->Size = 0;
    if (size > gpuBuffer->Size)
    {
        SDL_ReleaseGPUBuffer(gpuBuffer->Device, gpuBuffer->Buffer);
        gpuBuffer->Buffer = NULL;
        gpuBuffer->Capacity = 0;
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = gpuBuffer->Usage;
        info.size = cpuBuffer->Capacity * cpuBuffer->Stride;
        gpuBuffer->Buffer = SDL_CreateGPUBuffer(gpuBuffer->Device, &info);
        if (!gpuBuffer->Buffer)
        {
            SDL_Log("Failed to create buffer: %s", SDL_GetError());
            return;
        }
        gpuBuffer->Capacity = gpuBuffer->Capacity;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = cpuBuffer->Buffer;
    region.buffer = gpuBuffer->Buffer;
    region.size = size * cpuBuffer->Stride;
    SDL_UploadToGPUBuffer(pass, &location, &region, true);
    gpuBuffer->Size = size;
}
