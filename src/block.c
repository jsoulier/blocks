#include <stdbool.h>
#include "block.h"

const int blocks[][DIRECTION_3][2] =
{
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{2, 0}, {2, 0}, {2, 0}, {2, 0}, {1, 0}, {3, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
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