#include <SDL3/SDL.h>

#include <assert.h>

#include "block.h"
#include "direction.h"
#include "voxel.h"
#include "voxel_config.h"

static voxel_t pack(block_t block, int x, int y, int z, int u, int v, direction_t direction, int occlusion)
{
    static_assert(VOXEL_X_OFFSET + VOXEL_X_BITS <= 32, "");
    static_assert(VOXEL_Y_OFFSET + VOXEL_Y_BITS <= 32, "");
    static_assert(VOXEL_Z_OFFSET + VOXEL_Z_BITS <= 32, "");
    static_assert(VOXEL_U_OFFSET + VOXEL_U_BITS <= 32, "");
    static_assert(VOXEL_V_OFFSET + VOXEL_V_BITS <= 32, "");
    static_assert(VOXEL_FACE_OFFSET + VOXEL_FACE_BITS <= 32, "");
    static_assert(VOXEL_DIRECTION_OFFSET + VOXEL_DIRECTION_BITS <= 32, "");
    static_assert(VOXEL_OCCLUSION_OFFSET + VOXEL_OCCLUSION_BITS <= 32, "");
    int face = blocks[block][direction];
    SDL_assert(x <= VOXEL_X_MASK);
    SDL_assert(y <= VOXEL_Y_MASK);
    SDL_assert(z <= VOXEL_Z_MASK);
    SDL_assert(u <= VOXEL_U_MASK);
    SDL_assert(v <= VOXEL_V_MASK);
    SDL_assert(face <= VOXEL_FACE_MASK);
    SDL_assert(direction <= VOXEL_DIRECTION_MASK);
    SDL_assert(occlusion <= VOXEL_OCCLUSION_MASK);
    uint32_t voxel = 0;
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

voxel_t voxel_pack_sprite(block_t block, int x, int y, int z, direction_t direction, int i)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < 4);
    SDL_assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 0}, {0, 1, 0}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 1}, {0, 1, 0}, {1, 1, 1}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 1}, {1, 1, 0}},
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 0}, {1, 1, 0}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
    };
    int a = positions[direction][i][0] + x;
    int b = positions[direction][i][1] + y;
    int c = positions[direction][i][2] + z;
    int d = uvs[direction][i][0];
    int e = uvs[direction][i][1];
    return pack(block, a, b, c, d, e, DIRECTION_U, 0);
}

voxel_t voxel_pack_cube(block_t block, int x, int y, int z, direction_t direction, int occlusion, int i)
{
    SDL_assert(block > BLOCK_EMPTY);
    SDL_assert(block < BLOCK_COUNT);
    SDL_assert(direction < DIRECTION_3);
    SDL_assert(i < 4);
    static const int positions[][4][3] =
    {
        {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}},
        {{0, 1, 0}, {1, 1, 0}, {0, 1, 1}, {1, 1, 1}},
        {{0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {1, 0, 1}},
    };
    static const int uvs[][4][2] =
    {
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {0, 1}, {1, 0}, {0, 0}},
        {{1, 1}, {1, 0}, {0, 1}, {0, 0}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {0, 1}, {1, 0}, {1, 1}},
    };
    int a = positions[direction][i][0] + x;
    int b = positions[direction][i][1] + y;
    int c = positions[direction][i][2] + z;
    int d = uvs[direction][i][0];
    int e = uvs[direction][i][1];
    return pack(block, a, b, c, d, e, direction, occlusion);
}
