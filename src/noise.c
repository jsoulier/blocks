#include <SDL3/SDL.h>
#include <FastNoiseLite.h>

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

static void Stairs(const Noise* noise, Chunk* chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    for (int z = 0; z < CHUNK_WIDTH; z++)
    for (int y = 0; y < z; y++)
    {
        SetChunkBlock(chunk, chunk->X + x, chunk->Y + y, chunk->Z + z, BlockStone);
    }
}

static void Flat(const Noise* noise, Chunk* chunk)
{
    for (int x = 0; x < CHUNK_WIDTH; x++)
    for (int z = 0; z < CHUNK_WIDTH; z++)
    {
        for (int y = 0; y < 3; y++)
        {
            SetChunkBlock(chunk, chunk->X + x, chunk->Y + y, chunk->Z + z, BlockStone);
        }
        SetChunkBlock(chunk, chunk->X + x, chunk->Y + 3, chunk->Z + z, BlockDirt);
        SetChunkBlock(chunk, chunk->X + x, chunk->Y + 4, chunk->Z + z, BlockGrass);
    }
}

static void Terrain(const Noise* noise, Chunk* chunk)
{
    for (int i = 0; i < CHUNK_WIDTH; i++)
    for (int j = 0; j < CHUNK_WIDTH; j++)
    {
        float x = chunk->X + i;
        float z = chunk->Z + j;
    }
}

void GenerateChunkNoise(const Noise* noise, Chunk* chunk)
{
    switch (noise->Type)
    {
    case NoiseType1x1x1:
        _1x1x1(noise, chunk);
        break;
    case NoiseTypeStairs:
        Stairs(noise, chunk);
        break;
    case NoiseTypeFlat:
        Flat(noise, chunk);
        break;
    case NoiseTypeTerrain:
        Terrain(noise, chunk);
        break;
    }
}
