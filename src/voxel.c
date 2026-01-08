#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"
#include "voxel.h"
#include "voxel.inc"

static voxel_t pack(block_t block, int x, int y, int z, int u, int v, direction_t direction)
{
    SDL_COMPILE_TIME_ASSERT("", VOXEL_X_OFFSET + VOXEL_X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_U_OFFSET + VOXEL_U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_V_OFFSET + VOXEL_V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_INDEX_OFFSET + VOXEL_INDEX_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32);
    int index = block_get_index(block, direction);
    SDL_assert(x <= VOXEL_X_MASK);
    SDL_assert(y <= VOXEL_Y_MASK);
    SDL_assert(z <= VOXEL_Z_MASK);
    SDL_assert(u <= VOXEL_U_MASK);
    SDL_assert(v <= VOXEL_V_MASK);
    SDL_assert(index <= VOXEL_INDEX_MASK);
    SDL_assert(direction <= VOXEL_DIRECTION_MASK);
    voxel_t voxel = 0;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= u << VOXEL_U_OFFSET;
    voxel |= v << VOXEL_V_OFFSET;
    voxel |= index << VOXEL_INDEX_OFFSET;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    return voxel;
}

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(i < 4);
    static const int POSITIONS[][4][3] =
    {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    static const int TEXCOORDS[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    };
    int a = POSITIONS[direction][i][0] + x;
    int b = POSITIONS[direction][i][1] + y;
    int c = POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return pack(block, a, b, c, d, e, DIRECTION_UP);
}

voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int i)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 6);
    SDL_assert(i < 4);
    static const int POSITIONS[][4][3] =
    {
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
    };
    static const int TEXCOORDS[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    int a = POSITIONS[direction][i][0] + x;
    int b = POSITIONS[direction][i][1] + y;
    int c = POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return pack(block, a, b, c, d, e, direction);
}
