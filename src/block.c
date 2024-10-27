#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"

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

static bool opaque(const block_t block)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    switch (block)
    {
    case BLOCK_GLASS:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}

bool block_visible(const block_t a, const block_t b)
{
    return b == BLOCK_EMPTY || (opaque(a) && !opaque(b));
}