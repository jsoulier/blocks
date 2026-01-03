#include "block.h"
#include "direction.h"

struct
{
    int faces[DIRECTION_3];
}
static const blocks[BLOCK_COUNT] =
{
    [BLOCK_BLUEBELL] =
    {
        .faces = {13, 13, 13, 13, 13, 13}
    },
    [BLOCK_BUSH] =
    {
        .faces = {15, 15, 15, 15, 15, 15}
    },
    [BLOCK_CLOUD] =
    {
        .faces = { 9,  9,  9,  9,  9,  9}
    },
    [BLOCK_DANDELION] =
    {
        .faces = {12, 12, 12, 12, 12, 12}
    },
    [BLOCK_DIRT] =
    {
        .faces = { 3,  3,  3,  3,  3,  3}
    },
    [BLOCK_GRASS] =
    {
        .faces = { 2,  2,  2,  2,  1,  3}
    },
    [BLOCK_LAVENDER] =
    {
        .faces = {14, 14, 14, 14, 14, 14}
    },
    [BLOCK_LEAVES] =
    {
        .faces = {10, 10, 10, 10, 10, 10}
    },
    [BLOCK_LOG] =
    {
        .faces = { 8,  8,  8,  8,  7,  7}
    },
    [BLOCK_ROSE] =
    {
        .faces = {11, 11, 11, 11, 11, 11}
    },
    [BLOCK_SAND] =
    {
        .faces = { 5,  5,  5,  5,  5,  5}
    },
    [BLOCK_SNOW] =
    {
        .faces = { 6,  6,  6,  6,  6,  6}
    },
    [BLOCK_STONE] =
    {
        .faces = { 4,  4,  4,  4,  4,  4}
    },
    [BLOCK_WATER] =
    {
        .faces = {16, 16, 16, 16, 16, 16}
    },
};

int block_get_face(block_t block, direction_t direction)
{
    return blocks[block].faces[direction];
}