#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef void (*rand_set_block_t)(void* userdata, int bx, int by, int bz, block_t block);

void rand_get_blocks(void* userdata, int cx, int cz, rand_set_block_t function);
