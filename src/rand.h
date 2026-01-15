#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef void (*rand_set_blocks_cb_t)(void* userdata, int bx, int by, int bz, block_t block);

void rand_set_blocks(void* userdata, int cx, int cz, rand_set_blocks_cb_t cb);
