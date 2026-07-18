#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef void (*RandSetBlock)(void* userdata, int bx, int by, int bz, Block block);

void Rand_GetBlocks(void* userdata, int cx, int cz, RandSetBlock function);
