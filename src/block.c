#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "direction.h"

typedef struct BlockData
{
    Light light;
    Uint32 is_opaque;
    Uint32 is_solid;
    Uint32 is_sprite;
    Uint32 use_ao;
    Uint32 use_sun_normal;
    float sun_intensity;
    Uint32 indices[DIRECTION_COUNT];
} BlockData;

static const BlockData BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_EMPTY] =
    {
        .light = {0},
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .use_ao = false,
        .use_sun_normal = false,
        .sun_intensity = 0.0f,
        .indices = {0},
    },
    [BLOCK_GRASS] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {2, 2, 2, 2, 1, 3},
    },
    [BLOCK_DIRT] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {3, 3, 3, 3, 3, 3},
    },
    [BLOCK_SAND] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {5, 5, 5, 5, 5, 5},
    },
    [BLOCK_SNOW] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {6, 6, 6, 6, 6, 6},
    },
    [BLOCK_STONE] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {4, 4, 4, 4, 4, 4},
    },
    [BLOCK_LOG] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {8, 8, 8, 8, 7, 7},
    },
    [BLOCK_LEAVES] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {10, 10, 10, 10, 10, 10},
    },
    [BLOCK_CLOUD] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = false,
        .sun_intensity = 1.0f,
        .indices = {9, 9, 9, 9, 9, 9},
    },
    [BLOCK_BUSH] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {15, 15, 15, 15, 15, 15},
    },
    [BLOCK_BLUEBELL] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {13, 13, 13, 13, 13, 13},
    },
    [BLOCK_GARDENIA] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {12, 12, 12, 12, 12, 12},
    },
    [BLOCK_ROSE] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {11, 11, 11, 11, 11, 11},
    },
    [BLOCK_LAVENDER] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {14, 14, 14, 14, 14, 14},
    },
    [BLOCK_WATER] =
    {
        .light = {0},
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {16, 16, 16, 16, 16, 16},
    },
    [BLOCK_RED_TORCH] =
    {
        .light = {0, 0, 0, 236, 39, 63, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {17, 17, 17, 17, 17, 17},
    },
    [BLOCK_GREEN_TORCH] =
    {
        .light = {0, 0, 0, 90, 181, 82, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {18, 18, 18, 18, 18, 18},
    },
    [BLOCK_BLUE_TORCH] =
    {
        .light = {0, 0, 0, 51, 136, 222, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {19, 19, 19, 19, 19, 19},
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .light = {0, 0, 0, 243, 168, 51, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {20, 20, 20, 20, 20, 20},
    },
    [BLOCK_CYAN_TORCH] =
    {
        .light = {0, 0, 0, 54, 197, 244, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {21, 21, 21, 21, 21, 21},
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .light = {0, 0, 0, 250, 110, 121, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {22, 22, 22, 22, 22, 22},
    },
    [BLOCK_WHITE_TORCH] =
    {
        .light = {0, 0, 0, 255, 255, 255, 15},
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .use_ao = false,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {23, 23, 23, 23, 23, 23},
    },
    [BLOCK_PLANKS] =
    {
        .light = {0},
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .use_ao = true,
        .use_sun_normal = true,
        .sun_intensity = 0.55f,
        .indices = {24, 24, 24, 24, 24, 24},
    },
};

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(Light) == sizeof(Uint32) * 4);
    SDL_COMPILE_TIME_ASSERT("", sizeof(BlockData) == sizeof(Uint32) * 16);
    CPUBuffer cpu_blocks;
    GPUBuffer gpu_blocks;
    CPUBuffer_Init(&cpu_blocks, device, sizeof(BlockData));
    GPUBuffer_Init(&gpu_blocks, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    if (!GPUBuffer_BeginUpload(&gpu_blocks))
    {
        CPUBuffer_Free(&cpu_blocks);
        GPUBuffer_Free(&gpu_blocks);
        return NULL;
    }
    for (int block_index = 0; block_index < BLOCK_COUNT; block_index++)
    {
        CPUBuffer_Append(&cpu_blocks, &BLOCKS[block_index]);
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
