#pragma once

#include <SDL3/SDL.h>

typedef struct chunk chunk_t;

void noise_init(Uint32 seed);
void noise_generate(chunk_t* chunk, int x, int z);
