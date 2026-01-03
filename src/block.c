#include "block.h"
#include "direction.h"

struct
{
    int Faces[6];
}
static const kBlocks[BlockCount] =
{
    [BlockBluebell] =
    {
        .Faces = {13, 13, 13, 13, 13, 13}
    },
    [BlockBush] =
    {
        .Faces = {15, 15, 15, 15, 15, 15}
    },
    [BlockCloud] =
    {
        .Faces = { 9,  9,  9,  9,  9,  9}
    },
    [BlockDandelion] =
    {
        .Faces = {12, 12, 12, 12, 12, 12}
    },
    [BlockDirt] =
    {
        .Faces = { 3,  3,  3,  3,  3,  3}
    },
    [BlockGrass] =
    {
        .Faces = { 2,  2,  2,  2,  1,  3}
    },
    [BlockLavender] =
    {
        .Faces = {14, 14, 14, 14, 14, 14}
    },
    [BlockLeaves] =
    {
        .Faces = {10, 10, 10, 10, 10, 10}
    },
    [BlockLog] =
    {
        .Faces = { 8,  8,  8,  8,  7,  7}
    },
    [BlockRose] =
    {
        .Faces = {11, 11, 11, 11, 11, 11}
    },
    [BlockSand] =
    {
        .Faces = { 5,  5,  5,  5,  5,  5}
    },
    [BlockSnow] =
    {
        .Faces = { 6,  6,  6,  6,  6,  6}
    },
    [BlockStone] =
    {
        .Faces = { 4,  4,  4,  4,  4,  4}
    },
    [BlockWater] =
    {
        .Faces = {16, 16, 16, 16, 16, 16}
    },
};

int GetBlockFace(Block block, Direction direction)
{
    return kBlocks[block].Faces[direction];
}