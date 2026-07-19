#include <SDL3/SDL.h>

#include "block.h"
#include "buffer.h"
#include "direction.h"

typedef struct BlockData
{
    Uint32 is_opaque;
    Uint32 is_solid;
    Uint32 is_sprite;
    Uint32 is_occluded;
    Uint32 can_create_shadow;
    Uint32 can_be_in_shadow;
    float sun_intensity;
    Light light;
    Uint32 indices[DIRECTION_COUNT];
} BlockData;

static const BlockData BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_EMPTY] =
    {
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = false,
        .sun_intensity = 0.0f,
        .light = {0},
        .indices = {0},
    },
    [BLOCK_GRASS] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {2, 2, 2, 2, 1, 3},
    },
    [BLOCK_DIRT] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {3, 3, 3, 3, 3, 3},
    },
    [BLOCK_SAND] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {5, 5, 5, 5, 5, 5},
    },
    [BLOCK_SNOW] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {6, 6, 6, 6, 6, 6},
    },
    [BLOCK_STONE] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {4, 4, 4, 4, 4, 4},
    },
    [BLOCK_LOG] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {8, 8, 8, 8, 7, 7},
    },
    [BLOCK_LEAVES] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {10, 10, 10, 10, 10, 10},
    },
    [BLOCK_CLOUD] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = false,
        .can_be_in_shadow = false,
        .sun_intensity = 1.0f,
        .light = {0},
        .indices = {9, 9, 9, 9, 9, 9},
    },
    [BLOCK_BUSH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {15, 15, 15, 15, 15, 15},
    },
    [BLOCK_BLUEBELL] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {13, 13, 13, 13, 13, 13},
    },
    [BLOCK_GARDENIA] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {12, 12, 12, 12, 12, 12},
    },
    [BLOCK_ROSE] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {11, 11, 11, 11, 11, 11},
    },
    [BLOCK_LAVENDER] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {14, 14, 14, 14, 14, 14},
    },
    [BLOCK_WATER] =
    {
        .is_opaque = false,
        .is_solid = false,
        .is_sprite = false,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {16, 16, 16, 16, 16, 16},
    },
    [BLOCK_RED_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {236, 39, 63, 15},
        .indices = {17, 17, 17, 17, 17, 17},
    },
    [BLOCK_GREEN_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {90, 181, 82, 15},
        .indices = {18, 18, 18, 18, 18, 18},
    },
    [BLOCK_BLUE_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {51, 136, 222, 15},
        .indices = {19, 19, 19, 19, 19, 19},
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {243, 168, 51, 15},
        .indices = {20, 20, 20, 20, 20, 20},
    },
    [BLOCK_CYAN_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {54, 197, 244, 15},
        .indices = {21, 21, 21, 21, 21, 21},
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {250, 110, 121, 15},
        .indices = {22, 22, 22, 22, 22, 22},
    },
    [BLOCK_WHITE_TORCH] =
    {
        .is_opaque = true,
        .is_solid = false,
        .is_sprite = true,
        .is_occluded = false,
        .can_create_shadow = false,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {255, 255, 255, 15},
        .indices = {23, 23, 23, 23, 23, 23},
    },
    [BLOCK_PLANKS] =
    {
        .is_opaque = true,
        .is_solid = true,
        .is_sprite = false,
        .is_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .light = {0},
        .indices = {24, 24, 24, 24, 24, 24},
    },
};

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(Light) == sizeof(Uint32) * 4);
    SDL_COMPILE_TIME_ASSERT("", sizeof(BlockData) == sizeof(Uint32) * 17);
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

bool Block_IsOccluded(Block block)
{
    return BLOCKS[block].is_occluded;
}

bool Block_CanCreateShadow(Block block)
{
    return BLOCKS[block].can_create_shadow;
}

bool Block_CanBeInShadow(Block block)
{
    return BLOCKS[block].can_be_in_shadow;
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
