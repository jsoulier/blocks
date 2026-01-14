#include <SDL3/SDL.h>

#include "buffer.h"
#include "check.h"

static _Thread_local SDL_GPUCommandBuffer* command_buffer;
static _Thread_local SDL_GPUCopyPass* copy_pass;

bool gpu_begin_upload(SDL_GPUDevice* device)
{
    CHECK(!command_buffer);
    CHECK(!copy_pass);
    command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return false;
    }
    return true;
}

void gpu_end_upload(SDL_GPUDevice* device)
{
    CHECK(copy_pass);
    CHECK(command_buffer);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    copy_pass = NULL;
    command_buffer = NULL;
}

void cpu_buffer_init(cpu_buffer_t* cpu, SDL_GPUDevice* device, Uint32 stride)
{
    CHECK(stride);
    cpu->device = device;
    cpu->buffer = NULL;
    cpu->data = NULL;
    cpu->capacity = 0;
    cpu->size = 0;
    cpu->stride = stride;
}

void cpu_buffer_free(cpu_buffer_t* cpu)
{
    SDL_ReleaseGPUTransferBuffer(cpu->device, cpu->buffer);
    cpu->device = NULL;
    cpu->buffer = NULL;
    cpu->data = NULL;
    cpu->capacity = 0;
    cpu->size = 0;
    cpu->stride = 0;
}

void cpu_buffer_append(cpu_buffer_t* cpu, void* item)
{
    if (!cpu->data && cpu->buffer)
    {
        CHECK(!cpu->size);
        cpu->data = SDL_MapGPUTransferBuffer(cpu->device, cpu->buffer, true);
        if (!cpu->data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            return;
        }
    }
    CHECK(cpu->size <= cpu->capacity);
    if (cpu->size == cpu->capacity)
    {
        int capacity = SDL_max(64, cpu->size * 2);
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = capacity * cpu->stride;
        SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(cpu->device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return;
        }
        void* data = SDL_MapGPUTransferBuffer(cpu->device, buffer, false);
        if (!data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(cpu->device, buffer);
            return;
        }
        if (cpu->data)
        {
            SDL_memcpy(data, cpu->data, cpu->size * cpu->stride);
            SDL_UnmapGPUTransferBuffer(cpu->device, cpu->buffer);
        }
        SDL_ReleaseGPUTransferBuffer(cpu->device, cpu->buffer);
        cpu->capacity = capacity;
        cpu->buffer = buffer;
        cpu->data = data;
    }
    CHECK(cpu->data);
    SDL_memcpy(cpu->data + cpu->size * cpu->stride, item, cpu->stride);
    cpu->size++;
}

void cpu_buffer_clear(cpu_buffer_t* cpu)
{
    cpu->size = 0;
}

void gpu_buffer_init(gpu_buffer_t* gpu, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage)
{
    gpu->device = device;
    gpu->usage = usage;
    gpu->buffer = NULL;
    gpu->capacity = 0;
    gpu->size = 0;
}

void gpu_buffer_free(gpu_buffer_t* gpu)
{
    SDL_ReleaseGPUBuffer(gpu->device, gpu->buffer);
    gpu->usage = 0;
    gpu->buffer = NULL;
    gpu->capacity = 0;
    gpu->size = 0;
}

void gpu_buffer_upload(gpu_buffer_t* gpu, cpu_buffer_t* cpu)
{
    CHECK(command_buffer);
    CHECK(copy_pass);
    gpu->size = 0;
    if (cpu->data)
    {
        SDL_UnmapGPUTransferBuffer(gpu->device, cpu->buffer);
        cpu->data = NULL;
    }
    if (!cpu->size)
    {
        gpu->size = 0;
        return;
    }
    Uint32 size = cpu->size;
    cpu->size = 0;
    if (size > gpu->size)
    {
        SDL_ReleaseGPUBuffer(gpu->device, gpu->buffer);
        gpu->buffer = NULL;
        gpu->capacity = 0;
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = gpu->usage;
        info.size = cpu->capacity * cpu->stride;
        gpu->buffer = SDL_CreateGPUBuffer(gpu->device, &info);
        if (!gpu->buffer)
        {
            SDL_Log("Failed to create buffer: %s", SDL_GetError());
            return;
        }
        gpu->capacity = cpu->capacity;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = cpu->buffer;
    region.buffer = gpu->buffer;
    region.size = size * cpu->stride;
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, true);
    gpu->size = size;
}

void gpu_buffer_clear(gpu_buffer_t* gpu)
{
    gpu->size = 0;
}
