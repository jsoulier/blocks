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

static const int SPRITE_POSITIONS[][4][3] = {
    {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
    {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
    {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
};

static const int AO[2][4] = {{0, 1, 2, 3}, {1, 3, 0, 2}};

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

Voxel PackSprite(Block block, int x, int y, int z, Direction direction, int index)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(index < 4);
    const int* p = SPRITE_POSITIONS[direction][index];
    const int* t = TEXCOORDS[direction][index];
    return Pack(block, x + p[0], y + p[1], z + p[2], t[0], t[1], DIRECTION_UP, AO_MASK);
}

Voxel PackCube(Block block, int x, int y, int z, Direction direction, int index, int ao)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 6);
    SDL_assert(index < 4);
    const int* p = CUBE_POSITIONS[direction][index];
    const int* t = TEXCOORDS[direction][index];
    return Pack(block, x + p[0], y + p[1], z + p[2], t[0], t[1], direction, ao);
}

void GetAO(const int ao[4], int order[4])
{
    int index = ao[0] + ao[3] > ao[1] + ao[2];
    SDL_memcpy(order, AO[index], sizeof(AO[index]));
}

void GetPosition(Direction direction, int index, int position[3])
{
    SDL_assert(direction < 6);
    SDL_memcpy(position, CUBE_POSITIONS[direction][index], sizeof(int) * 3);
}
