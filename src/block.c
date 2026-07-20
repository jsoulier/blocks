#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "direction.h"

typedef struct Material
{
    Uint32 indices[DIRECTION_COUNT];
    Uint32 padding0;
    Uint32 padding1;
    Light light;
    Uint32 is_opaque;
    Uint32 is_solid;
    Uint32 is_sprite;
    Uint32 use_ao;
} Material;

static const Material BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_EMPTY] =
    {
        .indices = {0},
        .light = {0},
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .use_ao = false,
    },
    [BLOCK_GRASS] =
    {
        .indices = {2, 2, 2, 2, 1, 3},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_DIRT] =
    {
        .indices = {3, 3, 3, 3, 3, 3},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_SAND] =
    {
        .indices = {5, 5, 5, 5, 5, 5},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_SNOW] =
    {
        .indices = {6, 6, 6, 6, 6, 6},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_STONE] =
    {
        .indices = {4, 4, 4, 4, 4, 4},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_LOG] =
    {
        .indices = {8, 8, 8, 8, 7, 7},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_LEAVES] =
    {
        .indices = {10, 10, 10, 10, 10, 10},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_CLOUD] =
    {
        .indices = {9, 9, 9, 9, 9, 9},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
    [BLOCK_BUSH] =
    {
        .indices = {15, 15, 15, 15, 15, 15},
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_BLUEBELL] =
    {
        .indices = {13, 13, 13, 13, 13, 13},
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_GARDENIA] =
    {
        .indices = {12, 12, 12, 12, 12, 12},
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_ROSE] =
    {
        .indices = {11, 11, 11, 11, 11, 11},
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_LAVENDER] =
    {
        .indices = {14, 14, 14, 14, 14, 14},
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_WATER] =
    {
        .indices = {16, 16, 16, 16, 16, 16},
        .light = {0},
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .use_ao = false,
    },
    [BLOCK_RED_TORCH] =
    {
        .indices = {17, 17, 17, 17, 17, 17},
        .light = {0, 0, 0, 236, 39, 63, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_GREEN_TORCH] =
    {
        .indices = {18, 18, 18, 18, 18, 18},
        .light = {0, 0, 0, 90, 181, 82, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_BLUE_TORCH] =
    {
        .indices = {19, 19, 19, 19, 19, 19},
        .light = {0, 0, 0, 51, 136, 222, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .indices = {20, 20, 20, 20, 20, 20},
        .light = {0, 0, 0, 243, 168, 51, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_CYAN_TORCH] =
    {
        .indices = {21, 21, 21, 21, 21, 21},
        .light = {0, 0, 0, 54, 197, 244, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .indices = {22, 22, 22, 22, 22, 22},
        .light = {0, 0, 0, 250, 110, 121, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_WHITE_TORCH] =
    {
        .indices = {23, 23, 23, 23, 23, 23},
        .light = {0, 0, 0, 255, 255, 255, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
    },
    [BLOCK_PLANKS] =
    {
        .indices = {24, 24, 24, 24, 24, 24},
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
    },
};

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(Light) == sizeof(Uint32) * 4);
    SDL_COMPILE_TIME_ASSERT("", sizeof(Material) == sizeof(Uint32) * 16);
    CPUBuffer cpu_blocks;
    GPUBuffer gpu_blocks;
    CPUBuffer_Init(&cpu_blocks, device, sizeof(Material));
    GPUBuffer_Init(&gpu_blocks, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    if (!GPUBuffer_BeginUpload(&gpu_blocks))
    {
        CPUBuffer_Free(&cpu_blocks);
        GPUBuffer_Free(&gpu_blocks);
        return NULL;
    }
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        CPUBuffer_Append(&cpu_blocks, &BLOCKS[i]);
    }
    if (!GPUBuffer_Upload(&gpu_blocks, &cpu_blocks))
    {
        GPUBuffer_EndUpload();
        CPUBuffer_Free(&cpu_blocks);
        GPUBuffer_Free(&gpu_blocks);
        return NULL;
    }
    GPUBuffer_EndUpload();
    SDL_GPUBuffer* buffer = gpu_blocks.buffer;
    gpu_blocks.buffer = NULL;
    CPUBuffer_Free(&cpu_blocks);
    GPUBuffer_Free(&gpu_blocks);
    return buffer;
}

bool Block_IsOpaque(Block block)
{
    return BLOCKS[block].is_opaque;
}

bool Block_IsSolid(Block block)
{
    return BLOCKS[block].is_solid;
}

bool Block_IsSprite(Block block)
{
    return BLOCKS[block].is_sprite;
}

bool Block_UseAO(Block block)
{
    return BLOCKS[block].use_ao;
}

int Block_GetIndex(Block block, Direction direction)
{
    return BLOCKS[block].indices[direction];
}

bool Block_IsLight(Block block)
{
    return BLOCKS[block].light.radius > 0;
}

Light Block_GetLight(Block block)
{
    return BLOCKS[block].light;
}
