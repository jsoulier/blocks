#include <SDL3/SDL.h>

#include "block.h"
#include "direction.h"
#include "voxel.h"
#include "voxel_config.h"

static Voxel Pack(Block block, int x, int y, int z, int u, int v, Direction direction, int occlusion)
{
    SDL_COMPILE_TIME_ASSERT("", VOXEL_X_OFFSET + VOXEL_X_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_U_OFFSET + VOXEL_U_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_V_OFFSET + VOXEL_V_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_FACE_OFFSET + VOXEL_FACE_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32);
    SDL_COMPILE_TIME_ASSERT("", VOXEL_OCCLUSION_OFFSET + VOXEL_OCCLUSION_BITS <= 32);
    int face = GetBlockFace(block, direction);
    SDL_assert(x <= VOXEL_X_MASK);
    SDL_assert(y <= VOXEL_Y_MASK);
    SDL_assert(z <= VOXEL_Z_MASK);
    SDL_assert(u <= VOXEL_U_MASK);
    SDL_assert(v <= VOXEL_V_MASK);
    SDL_assert(face <= VOXEL_FACE_MASK);
    SDL_assert(direction <= VOXEL_DIRECTION_MASK);
    SDL_assert(occlusion <= VOXEL_OCCLUSION_MASK);
    Voxel voxel = 0;
    voxel |= x << VOXEL_X_OFFSET;
    voxel |= y << VOXEL_Y_OFFSET;
    voxel |= z << VOXEL_Z_OFFSET;
    voxel |= u << VOXEL_U_OFFSET;
    voxel |= v << VOXEL_V_OFFSET;
    voxel |= face << VOXEL_FACE_OFFSET;
    voxel |= direction << VOXEL_DIRECTION_OFFSET;
    voxel |= occlusion << VOXEL_OCCLUSION_OFFSET;
    return voxel;
}

Voxel VoxelPackSprite(Block block, int x, int y, int z, Direction direction, int i)
{
    SDL_assert(block > BlockEmpty);
    SDL_assert(block < BlockCount);
    SDL_assert(direction < 4);
    SDL_assert(i < 4);
    static const int kPositions[][4][3] =
    {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    static const int kTexcoords[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    };
    int a = kPositions[direction][i][0] + x;
    int b = kPositions[direction][i][1] + y;
    int c = kPositions[direction][i][2] + z;
    int d = kTexcoords[direction][i][0];
    int e = kTexcoords[direction][i][1];
    return Pack(block, a, b, c, d, e, DirectionUp, 0);
}

Voxel VoxelPackCube(Block block, int x, int y, int z, Direction direction, int occlusion, int i)
{
    SDL_assert(block > BlockEmpty);
    SDL_assert(block < BlockCount);
    SDL_assert(direction < 6);
    SDL_assert(i < 4);
    static const int kPositions[][4][3] =
    {
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
    };
    static const int kTexcoords[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    int a = kPositions[direction][i][0] + x;
    int b = kPositions[direction][i][1] + y;
    int c = kPositions[direction][i][2] + z;
    int d = kTexcoords[direction][i][0];
    int e = kTexcoords[direction][i][1];
    return Pack(block, a, b, c, d, e, direction, occlusion);
}
