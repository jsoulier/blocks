#pragma once

#include <SDL3/SDL.h>

SDL_GPUShader* LoadShader(SDL_GPUDevice* device, const char* path);
SDL_GPUComputePipeline* LoadComputePipeline(SDL_GPUDevice* device, const char* path);