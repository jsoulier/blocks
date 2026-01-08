#include <SDL3/SDL.h>
#include <FastNoiseLite.h>

#include "chunk.h"
#include "noise.h"

void noise_init(Uint32 seed)
{
}

void noise_generate(chunk_t* chunk, int x, int z)
{
    for (int i = 0; i < CHUNK_WIDTH; i++)
    for (int j = 0; j < CHUNK_WIDTH; j++)
    {
        for (int k = 0; k < 3; k++)
        {
            chunk_set_block(chunk, x + i, k, z + j, BLOCK_STONE);
        }
        chunk_set_block(chunk, x + i, 3, z + j, BLOCK_DIRT);
        chunk_set_block(chunk, x + i, 4, z + j, BLOCK_GRASS);
    }
}
