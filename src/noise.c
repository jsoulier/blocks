#include <stdint.h>

#include "chunk.h"
#include "noise.h"

void noise_init(noise_t* noise, noise_type_t type, uint32_t seed)
{
    noise->type = type;
    noise->seed = seed;
}

void noise_generate(const noise_t* noise, chunk_t* chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    for (int y = 0; y < CHUNK_WIDTH; y++)
    {

    }
}
