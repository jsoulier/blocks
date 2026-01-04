#include <SDL3/SDL.h>

#include "chunk.h"
#include "noise.h"

void CreateNoise(Noise* noise, NoiseType type, Uint32 seed)
{
    noise->Type = type;
    noise->Seed = seed;
}

static void _1x1x1(const Noise* noise, Chunk* chunk)
{
    SetChunkBlock(chunk, chunk->X, chunk->Y, chunk->Z, BlockGrass);
}

static void Terrain(const Noise* noise, Chunk* chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    for (int y = 0; y < CHUNK_WIDTH; y++)
    {

    }
}

void GenerateChunkNoise(const Noise* noise, Chunk* chunk)
{
    switch (noise->Type)
    {
    case NoiseType1x1x1:
        _1x1x1(noise, chunk);
        break;
    case NoiseTypeTerrain:
        Terrain(noise, chunk);
        break;
    }
}
