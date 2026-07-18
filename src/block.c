#include "block.h"
#include "buffer.h"
#include "direction.h"

#define TORCH_INTENSITY 15

struct
{
    bool is_opaque;
    bool is_sprite;
    bool is_solid;
    bool has_occlusion;
    bool is_fullbright;
    int indices[6];
    Light light;
}
static const BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_GRASS] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {2, 2, 2, 2, 1, 3},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_DIRT] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {3, 3, 3, 3, 3, 3},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_SAND] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {5, 5, 5, 5, 5, 5},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_SNOW] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {6, 6, 6, 6, 6, 6},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_STONE] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {4, 4, 4, 4, 4, 4},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LOG] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {8, 8, 8, 8, 7, 7},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LEAVES] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {10, 10, 10, 10, 10, 10},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_CLOUD] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = false,
        .is_fullbright = true,
        .indices = {9, 9, 9, 9, 9, 9},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_BUSH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {15, 15, 15, 15, 15, 15},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_BLUEBELL] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {13, 13, 13, 13, 13, 13},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_GARDENIA] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {12, 12, 12, 12, 12, 12},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_ROSE] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {11, 11, 11, 11, 11, 11},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LAVENDER] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {14, 14, 14, 14, 14, 14},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_WATER] =
    {
        .is_opaque = false,
        .is_sprite = false,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {16, 16, 16, 16, 16, 16},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_RED_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {17, 17, 17, 17, 17, 17},
        .light = {236, 39, 63, TORCH_INTENSITY},
    },
    [BLOCK_GREEN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {18, 18, 18, 18, 18, 18},
        .light = {90, 181, 82, TORCH_INTENSITY},
    },
    [BLOCK_BLUE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {19, 19, 19, 19, 19, 19},
        .light = {51, 136, 222, TORCH_INTENSITY},
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {20, 20, 20, 20, 20, 20},
        .light = {243, 168, 51, TORCH_INTENSITY},
    },
    [BLOCK_CYAN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {21, 21, 21, 21, 21, 21},
        .light = {54, 197, 244, TORCH_INTENSITY},
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {22, 22, 22, 22, 22, 22},
        .light = {250, 110, 121, TORCH_INTENSITY},
    },
    [BLOCK_WHITE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .indices = {23, 23, 23, 23, 23, 23},
        .light = {255, 255, 255, TORCH_INTENSITY},
    },
    [BLOCK_PLANKS] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .indices = {24, 24, 24, 24, 24, 24},
        .light = {0, 0, 0, 0},
    },
};

SDL_GPUBuffer* Block_GetBuffer(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(BlockGPU) == sizeof(Uint32) * 9);
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
        block.is_sprite = BLOCKS[i].is_sprite;
        block.has_occlusion = BLOCKS[i].has_occlusion;
        block.is_fullbright = BLOCKS[i].is_fullbright;
        for (int j = 0; j < DIRECTION_COUNT; j++)
        {
            block.indices[j] = BLOCKS[i].indices[j];
        }
        CPUBuffer_Append(&cpu_blocks, &block);
    }
    GPUBuffer_Upload(&gpu_blocks, &cpu_blocks);
    GPUBuffer_EndUpload(&gpu_blocks);
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
