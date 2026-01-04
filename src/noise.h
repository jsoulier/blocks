#pragma once

#include <SDL3/SDL.h>

typedef struct Chunk Chunk;

typedef enum NoiseType
{
    NoiseType1x1x1,
    NoiseTypeTerrain,
}
NoiseType;

typedef struct Noise
{
    NoiseType Type;
    Uint32 Seed;
}
Noise;

void CreateNoise(Noise* noise, NoiseType type, Uint32 seed);
void GenerateChunkNoise(const Noise* noise, Chunk* chunk);