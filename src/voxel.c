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
    SDL_COMPILE_TIME_ASSERT("", VOXEL_AO_OFFSET + VOXEL_AO_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_X_OFFSET + VOXEL_X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_U_OFFSET + VOXEL_U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_V_OFFSET + VOXEL_V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_BLOCK_OFFSET + VOXEL_BLOCK_BITS <= 32);
    SDL_assert(direction < DIRECTION_COUNT);
    SDL_assert(block <= VOXEL_BLOCK_MASK);
    SDL_assert(x <= VOXEL_X_MASK);
    SDL_assert(y <= VOXEL_Y_MASK);
    SDL_assert(z <= VOXEL_Z_MASK);
    SDL_assert(u <= VOXEL_U_MASK);
    SDL_assert(v <= VOXEL_V_MASK);
    SDL_assert(direction <= VOXEL_DIRECTION_MASK);
    SDL_assert(ao <= VOXEL_AO_MASK);
    Voxel voxel = 0;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    voxel |= block << VOXEL_BLOCK_OFFSET;
    voxel |= ao << VOXEL_AO_OFFSET;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= u << VOXEL_U_OFFSET;
    voxel |= v << VOXEL_V_OFFSET;
    return voxel;
}

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int index)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(index < 4);
    const int* p = SPRITE_POSITIONS[direction][index];
    const int* t = TEXCOORDS[direction][index];
    return Pack(block, x + p[0], y + p[1], z + p[2], t[0], t[1], DIRECTION_UP, VOXEL_AO_MASK);
}

Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int index, int ao)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 6);
    SDL_assert(index < 4);
    const int* p = CUBE_POSITIONS[direction][index];
    const int* t = TEXCOORDS[direction][index];
    return Pack(block, x + p[0], y + p[1], z + p[2], t[0], t[1], direction, ao);
}

void Voxel_GetAO(const int ao[4], int order[4])
{
    int index = ao[0] + ao[3] > ao[1] + ao[2];
    SDL_memcpy(order, AO[index], sizeof(AO[index]));
}

void Voxel_GetPosition(Direction direction, int index, int position[3])
{
    SDL_assert(direction < 6);
    SDL_memcpy(position, CUBE_POSITIONS[direction][index], sizeof(int) * 3);
}
