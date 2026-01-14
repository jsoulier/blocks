#pragma once

#include <SDL3/SDL.h>

typedef struct cpu_buffer
{
    SDL_GPUDevice* device;
    SDL_GPUTransferBuffer* buffer;
    Uint8* data;
    Uint32 size;
    Uint32 capacity;
    Uint32 stride;
}
cpu_buffer_t;

void cpu_buffer_init(cpu_buffer_t* cpu, SDL_GPUDevice* device, Uint32 stride);
void cpu_buffer_free(cpu_buffer_t* cpu);
void cpu_buffer_append(cpu_buffer_t* cpu, void* item);
void cpu_buffer_clear(cpu_buffer_t* cpu);

typedef struct gpu_buffer
{
    SDL_GPUDevice* device;
    SDL_GPUBufferUsageFlags usage;
    SDL_GPUBuffer* buffer;
    Uint32 size;
    Uint32 capacity;
}
gpu_buffer_t;

void gpu_buffer_init(gpu_buffer_t* gpu, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage);
void gpu_buffer_free(gpu_buffer_t* gpu);
void gpu_buffer_upload(gpu_buffer_t* gpu, cpu_buffer_t* cpu);
void gpu_buffer_clear(gpu_buffer_t* gpu);
bool gpu_buffer_begin_upload(SDL_GPUDevice* device);
void gpu_buffer_end_upload(SDL_GPUDevice* device);
