#pragma once

#include <SDL3/SDL.h>

SDL_GPUShader* shader_load(SDL_GPUDevice* device, const char* path);
SDL_GPUComputePipeline* shader_load_compute(SDL_GPUDevice* device, const char* path);