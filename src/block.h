#pragma once

#include <SDL3/SDL.h>

#include "direction.h"
#include "light.h"

typedef Uint8 Block;
enum /* Block */
{
    BlockEmpty,
    BlockGrass,
    BlockDirt,
    BlockSand,
    BlockSnow,
    BlockStone,
    BlockLog,
    BlockLeaves,
    BlockCloud,
    BlockBush,
    BlockBluebell,
    BlockDandelion,
    BlockRose,
    BlockLavender,
    BlockWater,
    BlockYellowTorch,
    BlockCount,
};

bool IsBlockOpaque(Block block);
bool IsBlockSprite(Block block);
int GetBlockFace(Block block, Direction direction);
bool IsBlockLightSource(Block block);
Light GetBlockLight(Block block);