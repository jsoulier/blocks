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

static Voxel Pack(
    Block block,
    int x,
    int y,
    int z,
    int texture_u,
    int texture_v,
    Direction direction,
    int ambient_occlusion)
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
    SDL_assert(texture_u <= VOXEL_U_MASK);
    SDL_assert(texture_v <= VOXEL_V_MASK);
    SDL_assert(direction <= VOXEL_DIRECTION_MASK);
    SDL_assert(ambient_occlusion <= VOXEL_AO_MASK);
    Voxel voxel = 0;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    voxel |= block << VOXEL_BLOCK_OFFSET;
    voxel |= ambient_occlusion << VOXEL_AO_OFFSET;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= texture_u << VOXEL_U_OFFSET;
    voxel |= texture_v << VOXEL_V_OFFSET;
    return voxel;
}

Voxel Voxel_PackSprite(Block block, int x, int y, int z, Direction direction, int vertex)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(vertex < 4);
    static const int POSITIONS[][4][3] = {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    const int* offset = POSITIONS[direction][vertex];
    const int* texcoord = TEXCOORDS[direction][vertex];
    return Pack(
        block,
        x + offset[0],
        y + offset[1],
        z + offset[2],
        texcoord[0],
        texcoord[1],
        DIRECTION_UP,
        VOXEL_AO_MASK);
}

Voxel Voxel_PackCube(Block block, int x, int y, int z, Direction direction, int vertex, int ambient_occlusion)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 6);
    SDL_assert(vertex < 4);
    const int* offset = CUBE_POSITIONS[direction][vertex];
    const int* texcoord = TEXCOORDS[direction][vertex];
    return Pack(
        block,
        x + offset[0],
        y + offset[1],
        z + offset[2],
        texcoord[0],
        texcoord[1],
        direction,
        ambient_occlusion);
}

void Voxel_GetCubePosition(Direction direction, int vertex, int position[3])
{
    SDL_assert(direction < 6);
    SDL_assert(vertex < 4);
    SDL_memcpy(position, CUBE_POSITIONS[direction][vertex], sizeof(int) * 3);
}
