#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"

void cpu_buffer_init(cpu_buffer_t* cpu_buffer, SDL_GPUDevice* device, uint32_t stride)
{
    cpu_buffer->device = device;
    cpu_buffer->buffer = NULL;
    cpu_buffer->data = NULL;
    cpu_buffer->capacity = 0;
    cpu_buffer->size = 0;
    cpu_buffer->stride = 0;
}

void cpu_buffer_free(cpu_buffer_t* cpu_buffer)
{
    SDL_ReleaseGPUTransferBuffer(cpu_buffer->device, cpu_buffer->buffer);
    cpu_buffer->device = NULL;
    cpu_buffer->buffer = NULL;
    cpu_buffer->data = NULL;
    cpu_buffer->capacity = 0;
    cpu_buffer->size = 0;
    cpu_buffer->stride = 0;
}

void cpu_buffer_add(cpu_buffer_t* cpu_buffer, void* item)
{
    uint32_t stride = cpu_buffer->stride;
    if (!cpu_buffer->data && cpu_buffer->buffer)
    {
        SDL_assert(!cpu_buffer->size);
        cpu_buffer->data = SDL_MapGPUTransferBuffer(cpu_buffer->device, cpu_buffer->buffer, true);
        if (!cpu_buffer->data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            return;
        }
    }
    SDL_assert(cpu_buffer->size <= cpu_buffer->capacity);
    if (cpu_buffer->size == cpu_buffer->capacity)
    {
        int capacity = SDL_max(64, cpu_buffer->size * 2);
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = capacity * stride;
        SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(cpu_buffer->device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return;
        }
        void* data = SDL_MapGPUTransferBuffer(cpu_buffer->device, buffer, false);
        if (!data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(cpu_buffer->device, buffer);
            return;
        }
        if (cpu_buffer->data)
        {
            memcpy(data, cpu_buffer->data, cpu_buffer->size * stride);
            SDL_UnmapGPUTransferBuffer(cpu_buffer->device, cpu_buffer->buffer);
        }
        SDL_ReleaseGPUTransferBuffer(cpu_buffer->device, cpu_buffer->buffer);
        cpu_buffer->capacity = capacity;
        cpu_buffer->buffer = buffer;
        cpu_buffer->data = data;
    }
    SDL_assert(cpu_buffer->data);
    memcpy(cpu_buffer->data + cpu_buffer->size * stride, item, stride);
    cpu_buffer->size++;
}

void gpu_buffer_init(gpu_buffer_t* gpu_buffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage)
{
    gpu_buffer->device = device;
    gpu_buffer->usage = usage;
    gpu_buffer->buffer = NULL;
    gpu_buffer->capacity = 0;
    gpu_buffer->size = 0;
}

void gpu_buffer_free(gpu_buffer_t* gpu_buffer)
{
    SDL_ReleaseGPUBuffer(gpu_buffer->device, gpu_buffer->buffer);
    gpu_buffer->buffer = NULL;
    gpu_buffer->capacity = 0;
    gpu_buffer->size = 0;
}

void gpu_buffer_upload(gpu_buffer_t* gpu_buffer, SDL_GPUCopyPass* pass, cpu_buffer_t* cpu_buffer)
{
    gpu_buffer->size = 0;
    if (cpu_buffer->data)
    {
        SDL_UnmapGPUTransferBuffer(gpu_buffer->device, cpu_buffer->buffer);
        cpu_buffer->data = NULL;
    }
    if (!cpu_buffer->size)
    {
        gpu_buffer->size = 0;
        return;
    }
    uint32_t size = cpu_buffer->size;
    cpu_buffer->size = 0;
    if (size > gpu_buffer->size)
    {
        SDL_ReleaseGPUBuffer(gpu_buffer->device, gpu_buffer->buffer);
        gpu_buffer->buffer = NULL;
        gpu_buffer->capacity = 0;
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = gpu_buffer->usage;
        info.size = cpu_buffer->capacity * cpu_buffer->stride;
        gpu_buffer->buffer = SDL_CreateGPUBuffer(gpu_buffer->device, &info);
        if (!gpu_buffer->buffer)
        {
            SDL_Log("Failed to create buffer: %s", SDL_GetError());
            return;
        }
        gpu_buffer->capacity = gpu_buffer->capacity;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = cpu_buffer->buffer;
    region.buffer = gpu_buffer->buffer;
    region.size = size * cpu_buffer->stride;
    SDL_UploadToGPUBuffer(pass, &location, &region, true);
    gpu_buffer->size = size;
}
