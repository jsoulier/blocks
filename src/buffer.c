#include <SDL3/SDL.h>

#include "buffer.h"

static _Thread_local SDL_GPUCommandBuffer* upload_command_buffer;
static _Thread_local SDL_GPUCopyPass* upload_pass;

void CPUBuffer_Init(CPUBuffer* buffer, SDL_GPUDevice* device, Uint32 stride)
{
    SDL_assert(stride);
    buffer->device = device;
    buffer->buffer = NULL;
    buffer->data = NULL;
    buffer->capacity = 0;
    buffer->size = 0;
    buffer->stride = stride;
}

void CPUBuffer_Free(CPUBuffer* buffer)
{
    SDL_ReleaseGPUTransferBuffer(buffer->device, buffer->buffer);
    buffer->device = NULL;
    buffer->buffer = NULL;
    buffer->data = NULL;
    buffer->capacity = 0;
    buffer->size = 0;
    buffer->stride = 0;
}

void CPUBuffer_Append(CPUBuffer* buffer, const void* item)
{
    if (!buffer->data && buffer->buffer)
    {
        SDL_assert(!buffer->size);
        buffer->data = SDL_MapGPUTransferBuffer(buffer->device, buffer->buffer, true);
        if (!buffer->data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            return;
        }
    }
    SDL_assert(buffer->size <= buffer->capacity);
    if (buffer->size == buffer->capacity)
    {
        int capacity = SDL_max(64, buffer->size * 2);
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = capacity * buffer->stride;
        SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(buffer->device, &info);
        if (!transfer_buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return;
        }
        void* data = SDL_MapGPUTransferBuffer(buffer->device, transfer_buffer, false);
        if (!data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(buffer->device, transfer_buffer);
            return;
        }
        if (buffer->data)
        {
            SDL_memcpy(data, buffer->data, buffer->size * buffer->stride);
            SDL_UnmapGPUTransferBuffer(buffer->device, buffer->buffer);
        }
        SDL_ReleaseGPUTransferBuffer(buffer->device, buffer->buffer);
        buffer->capacity = capacity;
        buffer->buffer = transfer_buffer;
        buffer->data = data;
    }
    SDL_assert(buffer->data);
    SDL_memcpy((Uint8*)buffer->data + buffer->size * buffer->stride, item, buffer->stride);
    buffer->size++;
}

void GPUBuffer_Init(GPUBuffer* buffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage)
{
    buffer->device = device;
    buffer->usage = usage;
    buffer->buffer = NULL;
    buffer->capacity = 0;
    buffer->size = 0;
}

void GPUBuffer_Free(GPUBuffer* buffer)
{
    SDL_ReleaseGPUBuffer(buffer->device, buffer->buffer);
    buffer->usage = 0;
    buffer->buffer = NULL;
    buffer->capacity = 0;
    buffer->size = 0;
}

bool GPUBuffer_Upload(GPUBuffer* destination, CPUBuffer* source)
{
    SDL_assert(upload_command_buffer);
    SDL_assert(upload_pass);
    destination->size = 0;
    if (source->data)
    {
        SDL_UnmapGPUTransferBuffer(destination->device, source->buffer);
        source->data = NULL;
    }
    if (!source->size)
    {
        return true;
    }
    Uint32 size = source->size;
    source->size = 0;
    if (size > destination->capacity)
    {
        SDL_ReleaseGPUBuffer(destination->device, destination->buffer);
        destination->buffer = NULL;
        destination->capacity = 0;
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = destination->usage;
        info.size = source->capacity * source->stride;
        destination->buffer = SDL_CreateGPUBuffer(destination->device, &info);
        if (!destination->buffer)
        {
            SDL_Log("Failed to create buffer: %s", SDL_GetError());
            return false;
        }
        destination->capacity = source->capacity;
    }
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = source->buffer;
    region.buffer = destination->buffer;
    region.size = size * source->stride;
    SDL_UploadToGPUBuffer(upload_pass, &location, &region, true);
    destination->size = size;
    return true;
}

void GPUBuffer_Clear(GPUBuffer* buffer)
{
    buffer->size = 0;
}

bool GPUBuffer_BeginUpload(GPUBuffer* buffer)
{
    SDL_assert(!upload_command_buffer);
    SDL_assert(!upload_pass);
    upload_command_buffer = SDL_AcquireGPUCommandBuffer(buffer->device);
    if (!upload_command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    upload_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
    if (!upload_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(upload_command_buffer);
        upload_command_buffer = NULL;
        return false;
    }
    return true;
}

void GPUBuffer_EndUpload()
{
    SDL_assert(upload_pass);
    SDL_assert(upload_command_buffer);
    SDL_EndGPUCopyPass(upload_pass);
    SDL_SubmitGPUCommandBuffer(upload_command_buffer);
    upload_pass = NULL;
    upload_command_buffer = NULL;
}
