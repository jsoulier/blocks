#include "block.h"
#include "direction.h"

struct
{
    bool Opaque;
    bool Sprite;
    int Faces[6];
}
static const kBlocks[BlockCount] =
{
    [BlockBluebell] =
    {
        .Opaque = true,
        .Sprite = true,
        .Faces = {13, 13, 13, 13, 13, 13}
    },
    [BlockBush] =
    {
        .Opaque = true,
        .Sprite = true,
        .Faces = {15, 15, 15, 15, 15, 15}
    },
    [BlockCloud] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 9,  9,  9,  9,  9,  9}
    },
    [BlockDandelion] =
    {
        .Opaque = true,
        .Sprite = true,
        .Faces = {12, 12, 12, 12, 12, 12}
    },
    [BlockDirt] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 3,  3,  3,  3,  3,  3}
    },
    [BlockGrass] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 2,  2,  2,  2,  1,  3}
    },
    [BlockLavender] =
    {
        .Opaque = true,
        .Sprite = true,
        .Faces = {14, 14, 14, 14, 14, 14}
    },
    [BlockLeaves] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = {10, 10, 10, 10, 10, 10}
    },
    [BlockLog] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 8,  8,  8,  8,  7,  7}
    },
    [BlockRose] =
    {
        .Opaque = true,
        .Sprite = true,
        .Faces = {11, 11, 11, 11, 11, 11}
    },
    [BlockSand] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 5,  5,  5,  5,  5,  5}
    },
    [BlockSnow] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 6,  6,  6,  6,  6,  6}
    },
    [BlockStone] =
    {
        .Opaque = true,
        .Sprite = false,
        .Faces = { 4,  4,  4,  4,  4,  4}
    },
    [BlockWater] =
    {
        .Opaque = false,
        .Sprite = false,
        .Faces = {16, 16, 16, 16, 16, 16}
    },
};

bool IsBlockOpaque(Block block)
{
    return kBlocks[block].Opaque;
}

bool IsBlockSprite(Block block)
{
    return kBlocks[block].Sprite;
}

int GetBlockFace(Block block, Direction direction)
{
    return kBlocks[block].Faces[direction];
}
