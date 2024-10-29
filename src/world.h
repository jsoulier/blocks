#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include "camera.h"
#include "helpers.h"

bool world_init(SDL_GPUDevice* device);
void world_free();
void world_update(
    const int32_t x,
    const int32_t y,
    const int32_t z);
void world_render(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass);
void world_set_block(
    const int32_t x,
    const int32_t y,
    const int32_t z,
    const block_t block);
block_t world_get_block(
    const int32_t x,
    const int32_t y,
    const int32_t z);