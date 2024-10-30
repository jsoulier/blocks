#include <stdbool.h>
#include "block.h"

const int blocks[][DIRECTION_3][2] =
{
    [BLOCK_CLOUD] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    [BLOCK_DIRT]  = {{3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}},
    [BLOCK_GLASS] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    [BLOCK_GRASS] = {{2, 0}, {2, 0}, {2, 0}, {2, 0}, {1, 0}, {3, 0}},
    [BLOCK_LAVA]  = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    [BLOCK_SAND]  = {{5, 0}, {5, 0}, {5, 0}, {5, 0}, {5, 0}, {5, 0}},
    [BLOCK_SNOW]  = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    [BLOCK_STONE] = {{4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}},
    [BLOCK_WATER] = {{1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}, {1, 1}},
};


bool block_opaque(const block_t block)
{
    switch (block) {
    case BLOCK_GLASS:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}

bool block_collision(const block_t block)
{
    switch (block) {
    case BLOCK_EMPTY:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}