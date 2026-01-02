#pragma once

#include <SDL3/SDL.h>

#include <stdint.h>

typedef struct cpu_buffer
{
    SDL_GPUTransferBufferUsage usage;
    SDL_GPUTransferBuffer* buffer;
    uint32_t size;
    uint32_t capacity;
    uint32_t stride;
}
cpu_buffer_t;

void cpu_buffer_init(cpu_buffer_t* cpu_buffer, SDL_GPUDevice* device, uint32_t stride);
void cpu_buffer_free(cpu_buffer_t* cpu_buffer, SDL_GPUDevice* device);
void cpu_buffer_add(cpu_buffer_t* cpu_buffer, SDL_GPUDevice* device, void* item);

typedef struct gpu_buffer
{
    SDL_GPUBufferUsageFlags usage;
    SDL_GPUBuffer* buffer;
    uint32_t size;
    uint32_t capacity;
}
gpu_buffer_t;

void gpu_buffer_init(gpu_buffer_t* gpu_buffer, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage);
void gpu_buffer_free(gpu_buffer_t* gpu_buffer, SDL_GPUDevice* device);
void gpu_buffer_upload(gpu_buffer_t* gpu_buffer, SDL_GPUDevice* device, SDL_GPUCopyPass* pass, cpu_buffer_t* cpu_buffer);
