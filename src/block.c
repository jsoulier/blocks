#include "block.h"
#include "buffer.h"
#include "direction.h"

#define TORCH_INTENSITY 15

typedef struct BlockGPU
{
    Uint32 is_sprite;
    Uint32 has_occlusion;
    Uint32 has_shadow;
    Uint32 can_be_in_shadow;
    float block_sun_intensity;
    Uint32 indices[DIRECTION_COUNT];
} BlockGPU;

struct
{
    bool is_opaque;
    bool is_sprite;
    bool is_solid;
    bool has_occlusion;
    bool has_shadow;
    bool can_be_in_shadow;
    float sun_intensity;
    int indices[6];
    Light light;
} static const BLOCKS[BLOCK_COUNT] = {
    [BLOCK_GRASS] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {2, 2, 2, 2, 1, 3},
        },
    [BLOCK_DIRT] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {3, 3, 3, 3, 3, 3},
        },
    [BLOCK_SAND] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {5, 5, 5, 5, 5, 5},
        },
    [BLOCK_SNOW] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {6, 6, 6, 6, 6, 6},
        },
    [BLOCK_STONE] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {4, 4, 4, 4, 4, 4},
        },
    [BLOCK_LOG] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {8, 8, 8, 8, 7, 7},
        },
    [BLOCK_LEAVES] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {10, 10, 10, 10, 10, 10},
        },
    [BLOCK_CLOUD] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .sun_intensity = 1.0f,
            .indices = {9, 9, 9, 9, 9, 9},
        },
    [BLOCK_BUSH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {15, 15, 15, 15, 15, 15},
        },
    [BLOCK_BLUEBELL] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {13, 13, 13, 13, 13, 13},
        },
    [BLOCK_GARDENIA] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {12, 12, 12, 12, 12, 12},
        },
    [BLOCK_ROSE] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {11, 11, 11, 11, 11, 11},
        },
    [BLOCK_LAVENDER] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {14, 14, 14, 14, 14, 14},
        },
    [BLOCK_WATER] =
        {
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {16, 16, 16, 16, 16, 16},
        },
    [BLOCK_RED_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {17, 17, 17, 17, 17, 17},
            .light = {236, 39, 63, TORCH_INTENSITY},
        },
    [BLOCK_GREEN_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {18, 18, 18, 18, 18, 18},
            .light = {90, 181, 82, TORCH_INTENSITY},
        },
    [BLOCK_BLUE_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {19, 19, 19, 19, 19, 19},
            .light = {51, 136, 222, TORCH_INTENSITY},
        },
    [BLOCK_YELLOW_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {20, 20, 20, 20, 20, 20},
            .light = {243, 168, 51, TORCH_INTENSITY},
        },
    [BLOCK_CYAN_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {21, 21, 21, 21, 21, 21},
            .light = {54, 197, 244, TORCH_INTENSITY},
        },
    [BLOCK_MAGENTA_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {22, 22, 22, 22, 22, 22},
            .light = {250, 110, 121, TORCH_INTENSITY},
        },
    [BLOCK_WHITE_TORCH] =
        {
            .is_opaque = true,
            .is_sprite = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {23, 23, 23, 23, 23, 23},
            .light = {255, 255, 255, TORCH_INTENSITY},
        },
    [BLOCK_PLANKS] =
        {
            .is_opaque = true,
            .is_solid = true,
            .has_occlusion = true,
            .has_shadow = true,
            .can_be_in_shadow = true,
            .sun_intensity = 0.4f,
            .indices = {24, 24, 24, 24, 24, 24},
        },
};

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(BlockGPU) == sizeof(Uint32) * 11);
    CPUBuffer cpu_blocks;
    GPUBuffer gpu_blocks;
    CPUBuffer_Init(&cpu_blocks, device, sizeof(BlockGPU));
    GPUBuffer_Init(&gpu_blocks, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    if (!GPUBuffer_BeginUpload(&gpu_blocks))
    {
        CPUBuffer_Free(&cpu_blocks);
        GPUBuffer_Free(&gpu_blocks);
        return NULL;
    }
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        BlockGPU block = {0};
        block.is_sprite = Block_IsSprite(i);
        block.has_occlusion = Block_HasOcclusion(i);
        block.has_shadow = Block_HasShadow(i);
        block.can_be_in_shadow = Block_CanBeInShadow(i);
        block.block_sun_intensity = BLOCKS[i].sun_intensity;
        for (int j = 0; j < DIRECTION_COUNT; j++)
        {
            block.indices[j] = BLOCKS[i].indices[j];
        }
        CPUBuffer_Append(&cpu_blocks, &block);
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

bool Block_IsSprite(Block block)
{
    return BLOCKS[block].is_sprite;
}

bool Block_IsSolid(Block block)
{
    return BLOCKS[block].is_solid;
}

bool Block_HasOcclusion(Block block)
{
    return BLOCKS[block].has_occlusion;
}

bool Block_HasShadow(Block block)
{
    return BLOCKS[block].has_shadow;
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
