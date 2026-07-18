#include "block.h"
#include "buffer.h"
#include "direction.h"

typedef struct BlockData
{
    Uint32 is_opaque;
    Uint32 is_solid;
    Uint32 is_sprite;
    Uint32 can_be_occluded;
    Uint32 can_create_shadow;
    Uint32 can_be_in_shadow;
    float sun_intensity;
    Light light;
    Uint32 indices[DIRECTION_COUNT];
} BlockData;

static const BlockData BLOCKS[BLOCK_COUNT] = {
    [BLOCK_GRASS] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {2, 2, 2, 2, 1, 3},
    },
    [BLOCK_DIRT] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {3, 3, 3, 3, 3, 3},
    },
    [BLOCK_SAND] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {5, 5, 5, 5, 5, 5},
    },
    [BLOCK_SNOW] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {6, 6, 6, 6, 6, 6},
    },
    [BLOCK_STONE] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {4, 4, 4, 4, 4, 4},
    },
    [BLOCK_LOG] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {8, 8, 8, 8, 7, 7},
    },
    [BLOCK_LEAVES] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {10, 10, 10, 10, 10, 10},
    },
    [BLOCK_CLOUD] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .sun_intensity = 1.0f,
        .indices = {9, 9, 9, 9, 9, 9},
    },
    [BLOCK_BUSH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {15, 15, 15, 15, 15, 15},
    },
    [BLOCK_BLUEBELL] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {13, 13, 13, 13, 13, 13},
    },
    [BLOCK_GARDENIA] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {12, 12, 12, 12, 12, 12},
    },
    [BLOCK_ROSE] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {11, 11, 11, 11, 11, 11},
    },
    [BLOCK_LAVENDER] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {14, 14, 14, 14, 14, 14},
    },
    [BLOCK_WATER] =
    {
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {16, 16, 16, 16, 16, 16},
    },
    [BLOCK_RED_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {17, 17, 17, 17, 17, 17},
        .light = {236, 39, 63, 15},
    },
    [BLOCK_GREEN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {18, 18, 18, 18, 18, 18},
        .light = {90, 181, 82, 15},
    },
    [BLOCK_BLUE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {19, 19, 19, 19, 19, 19},
        .light = {51, 136, 222, 15},
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {20, 20, 20, 20, 20, 20},
        .light = {243, 168, 51, 15},
    },
    [BLOCK_CYAN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {21, 21, 21, 21, 21, 21},
        .light = {54, 197, 244, 15},
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {22, 22, 22, 22, 22, 22},
        .light = {250, 110, 121, 15},
    },
    [BLOCK_WHITE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
        .indices = {23, 23, 23, 23, 23, 23},
        .light = {255, 255, 255, 15},

    },
    [BLOCK_PLANKS] =
    {
        .is_opaque = true,
        .is_solid = true,
        .can_be_occluded = true,
        .can_create_shadow = true,
        .can_be_in_shadow = true,
        .sun_intensity = 0.55f,
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
    GPUBuffer_Upload(&gpu_blocks, &cpu_blocks);
    GPUBuffer_EndUpload();
    SDL_GPUBuffer* buffer = gpu_blocks.size == BLOCK_COUNT ? gpu_blocks.buffer : NULL;
    if (buffer)
    {
        gpu_blocks.buffer = NULL;
    }
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

bool Block_CanBeOccluded(Block block)
{
    return BLOCKS[block].can_be_occluded;
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
