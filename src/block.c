#include "block.h"
#include "direction.h"

const int blocks[][DIRECTION_3] =
{
    [BLOCK_BLUEBELL]  = {13, 13, 13, 13, 13, 13},
    [BLOCK_BUSH]      = {15, 15, 15, 15, 15, 15},
    [BLOCK_CLOUD]     = { 9,  9,  9,  9,  9,  9},
    [BLOCK_DANDELION] = {12, 12, 12, 12, 12, 12},
    [BLOCK_DIRT]      = { 3,  3,  3,  3,  3,  3},
    [BLOCK_GRASS]     = { 2,  2,  2,  2,  1,  3},
    [BLOCK_LAVENDER]  = {14, 14, 14, 14, 14, 14},
    [BLOCK_LEAVES]    = {10, 10, 10, 10, 10, 10},
    [BLOCK_LOG]       = { 8,  8,  8,  8,  7,  7},
    [BLOCK_ROSE]      = {11, 11, 11, 11, 11, 11},
    [BLOCK_SAND]      = { 5,  5,  5,  5,  5,  5},
    [BLOCK_SNOW]      = { 6,  6,  6,  6,  6,  6},
    [BLOCK_STONE]     = { 4,  4,  4,  4,  4,  4},
    [BLOCK_WATER]     = {16, 16, 16, 16, 16, 16},
};