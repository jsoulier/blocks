#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"
#include "voxel.h"
#include "voxel.inc"

static const int CUBE_POSITIONS[][4][3] = {
    {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
    {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
    {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
    {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
    {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
    {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
};

static const int TEXCOORDS[][4][2] = {
    {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
    {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
    {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
    {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
};

static Voxel Pack(Block block, int x, int y, int z, int u, int v, Direction direction, int ao)
{
    SDL_COMPILE_TIME_ASSERT("", AO_OFFSET + AO_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", X_OFFSET + X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Y_OFFSET + Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", Z_OFFSET + Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", U_OFFSET + U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", V_OFFSET + V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", DIRECTION_OFFSET + DIRECTION_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", BLOCK_OFFSET + BLOCK_BITS <= 32);
    SDL_assert(direction < DIRECTION_COUNT);
    SDL_assert(block <= BLOCK_MASK);
    SDL_assert(x <= X_MASK);
    SDL_assert(y <= Y_MASK);
    SDL_assert(z <= Z_MASK);
    SDL_assert(u <= U_MASK);
    SDL_assert(v <= V_MASK);
    SDL_assert(direction <= DIRECTION_MASK);
    SDL_assert(ao <= AO_MASK);
    Voxel voxel = 0;
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

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int i)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(i < 4);
    static const int POSITIONS[][4][3] = {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    int a = POSITIONS[direction][i][0] + x;
    int b = POSITIONS[direction][i][1] + y;
    int c = POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return Pack(block, a, b, c, d, e, direction, AO_MASK);
}

Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int i, int ao)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 6);
    SDL_assert(i < 4);
    int a = CUBE_POSITIONS[direction][i][0] + x;
    int b = CUBE_POSITIONS[direction][i][1] + y;
    int c = CUBE_POSITIONS[direction][i][2] + z;
    int d = TEXCOORDS[direction][i][0];
    int e = TEXCOORDS[direction][i][1];
    return Pack(block, a, b, c, d, e, direction, ao);
}

void Voxel_GetCubePosition(Direction direction, int i, int position[3])
{
    SDL_assert(direction < 6);
    SDL_assert(i < 4);
    SDL_memcpy(position, CUBE_POSITIONS[direction][i], sizeof(int) * 3);
}
