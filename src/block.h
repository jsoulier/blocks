#pragma once

#include <SDL3/SDL.h>

#include "direction.h"

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
    BlockCount,
};

int GetBlockFace(Block block, Direction direction);