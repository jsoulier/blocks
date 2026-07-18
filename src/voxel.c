#include <SDL3/SDL.h>

#include "block.h"
#include "check.h"
#include "direction.h"
#include "voxel.h"
#include "voxel.inc"

static const int CUBE_POSITIONS[][4][3] =
{
    {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
    {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
    {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
    {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
    {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
    {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
};

static voxel_t pack(block_t block, int x, int y, int z, int u, int v, direction_t direction, int ao)
{
    SDL_COMPILE_TIME_ASSERT("", AO_OFFSET + AO_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", X_OFFSET + X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Y_OFFSET + Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Z_OFFSET + Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", U_OFFSET + U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", V_OFFSET + V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", DIRECTION_OFFSET + DIRECTION_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", BLOCK_OFFSET + BLOCK_BITS <= 32);
    CHECK(direction < DIRECTION_COUNT);
    CHECK(block <= BLOCK_MASK);
    CHECK(x <= X_MASK);
    CHECK(y <= Y_MASK);
    CHECK(z <= Z_MASK);
    CHECK(u <= U_MASK);
    CHECK(v <= V_MASK);
    CHECK(direction <= DIRECTION_MASK);
    CHECK(ao <= AO_MASK);
    voxel_t voxel = 0;
    voxel |= direction << DIRECTION_OFFSET;
    voxel |= block << BLOCK_OFFSET;
    voxel |= ao << AO_OFFSET;
    voxel |= x << X_OFFSET;
    voxel |= y << Y_OFFSET;
    voxel |= z << Z_OFFSET;
    voxel |= u << U_OFFSET;
    voxel |= v << V_OFFSET;
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
    int a = POSITIONS[direction][i][0] + x;
    int b = POSITIONS[direction][i][1] + y;
    int c = POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return pack(block, a, b, c, d, e, direction, AO_MASK);
}

voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int i, int ao)
{
    CHECK(block > BLOCK_EMPTY);
    CHECK(block < BLOCK_COUNT);
    CHECK(direction < 6);
    CHECK(i < 4);
    static const int TEXCOORDS[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    int a = CUBE_POSITIONS[direction][i][0] + x;
    int b = CUBE_POSITIONS[direction][i][1] + y;
    int c = CUBE_POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return pack(block, a, b, c, d, e, direction, ao);
}

void voxel_get_cube_position(direction_t direction, int i, int position[3])
{
    CHECK(direction < 6);
    CHECK(i < 4);
    SDL_memcpy(position, CUBE_POSITIONS[direction][i], sizeof(int) * 3);
}
