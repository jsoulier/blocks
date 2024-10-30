#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "block.h"
#include "camera.h"
#include "helpers.h"

bool world_init(SDL_GPUDevice* device);
void world_free();
void world_update(
    const int x,
    const int y,
    const int z);
void world_render_opaque(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass);
void world_render_transp(
    const camera_t* camera,
    SDL_GPUCommandBuffer* commands,
    SDL_GPURenderPass* pass);
void world_set_block(
    const int x,
    const int y,
    const int z,
    const block_t block);
block_t world_get_block(
    const int x,
    const int y,
    const int z);