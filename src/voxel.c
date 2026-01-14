#include <SDL3/SDL.h>

#include "block.h"
#include "check.h"
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
    CHECK(x <= VOXEL_X_MASK);
    CHECK(y <= VOXEL_Y_MASK);
    CHECK(z <= VOXEL_Z_MASK);
    CHECK(u <= VOXEL_U_MASK);
    CHECK(v <= VOXEL_V_MASK);
    CHECK(index <= VOXEL_INDEX_MASK);
    CHECK(direction <= VOXEL_DIRECTION_MASK);
    voxel_t voxel = 0;
    voxel |= block_is_occluded(block) << VOXEL_OCCLUSION_OFFSET;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    voxel |= block_has_shadow(block) << VOXEL_SHADOW_OFFSET;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= u << VOXEL_U_OFFSET;
    voxel |= v << VOXEL_V_OFFSET;
    voxel |= index << VOXEL_INDEX_OFFSET;
    return voxel;
}

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i)
{
    CHECK(block > BLOCK_EMPTY);
    CHECK(block < BLOCK_COUNT);
    CHECK(direction < 4);
    CHECK(i < 4);
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
    // TODO: adjusted normals. need another 4 directions (6/8 used)
    int a = POSITIONS[direction][i][0] + x;
    int b = POSITIONS[direction][i][1] + y;
    int c = POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return pack(block, a, b, c, d, e, DIRECTION_UP);
}

voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int i)
{
    CHECK(block > BLOCK_EMPTY);
    CHECK(block < BLOCK_COUNT);
    CHECK(direction < 6);
    CHECK(i < 4);
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
