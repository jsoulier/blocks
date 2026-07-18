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
    bool has_shadow;
    bool is_fullbright;
    int indices[6];
    light_t light;
}
static const BLOCKS[BLOCK_COUNT] =
{
    [BLOCK_GRASS] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {2, 2, 2, 2, 1, 3},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_DIRT] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {3, 3, 3, 3, 3, 3},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_SAND] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {5, 5, 5, 5, 5, 5},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_SNOW] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {6, 6, 6, 6, 6, 6},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_STONE] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {4, 4, 4, 4, 4, 4},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LOG] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {8, 8, 8, 8, 7, 7},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LEAVES] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {10, 10, 10, 10, 10, 10},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_CLOUD] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = false,
        .has_shadow = false,
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
        .has_shadow = false,
        .indices = {15, 15, 15, 15, 15, 15},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_BLUEBELL] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {13, 13, 13, 13, 13, 13},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_GARDENIA] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {12, 12, 12, 12, 12, 12},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_ROSE] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {11, 11, 11, 11, 11, 11},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_LAVENDER] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {14, 14, 14, 14, 14, 14},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_WATER] =
    {
        .is_opaque = false,
        .is_sprite = false,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {16, 16, 16, 16, 16, 16},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_RED_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {17, 17, 17, 17, 17, 17},
        .light = {236, 39, 63, TORCH_INTENSITY},
    },
    [BLOCK_GREEN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {18, 18, 18, 18, 18, 18},
        .light = {90, 181, 82, TORCH_INTENSITY},
    },
    [BLOCK_BLUE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {19, 19, 19, 19, 19, 19},
        .light = {51, 136, 222, TORCH_INTENSITY},
    },
    [BLOCK_YELLOW_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {20, 20, 20, 20, 20, 20},
        .light = {243, 168, 51, TORCH_INTENSITY},
    },
    [BLOCK_CYAN_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {21, 21, 21, 21, 21, 21},
        .light = {54, 197, 244, TORCH_INTENSITY},
    },
    [BLOCK_MAGENTA_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {22, 22, 22, 22, 22, 22},
        .light = {250, 110, 121, TORCH_INTENSITY},
    },
    [BLOCK_WHITE_TORCH] =
    {
        .is_opaque = true,
        .is_sprite = true,
        .is_solid = false,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {23, 23, 23, 23, 23, 23},
        .light = {255, 255, 255, TORCH_INTENSITY},
    },
    [BLOCK_PLANKS] =
    {
        .is_opaque = true,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = true,
        .has_shadow = true,
        .indices = {24, 24, 24, 24, 24, 24},
        .light = {0, 0, 0, 0},
    },
    [BLOCK_GLASS] =
    {
        .is_opaque = false,
        .is_sprite = false,
        .is_solid = true,
        .has_occlusion = false,
        .has_shadow = false,
        .indices = {25, 25, 25, 25, 25, 25},
        .light = {0, 0, 0, 0},
    },
};

static cpu_buffer_t cpu_blocks;
static gpu_buffer_t gpu_blocks;

bool block_init(SDL_GPUDevice* device)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(block_gpu_t) == sizeof(Uint32) * 10);
    cpu_buffer_init(&cpu_blocks, device, sizeof(block_gpu_t));
    gpu_buffer_init(&gpu_blocks, device, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ);
    if (!gpu_buffer_begin_upload(&gpu_blocks))
    {
        return false;
    }
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        block_gpu_t block = {0};
        block.is_sprite = BLOCKS[i].is_sprite;
        block.has_occlusion = BLOCKS[i].has_occlusion;
        block.has_shadow = BLOCKS[i].has_shadow;
        block.is_fullbright = BLOCKS[i].is_fullbright;
        for (int j = 0; j < DIRECTION_COUNT; j++)
        {
            block.indices[j] = BLOCKS[i].indices[j];
        }
        cpu_buffer_append(&cpu_blocks, &block);
    }
    gpu_buffer_upload(&gpu_blocks, &cpu_blocks);
    gpu_buffer_end_upload(&gpu_blocks);
    return gpu_blocks.size == BLOCK_COUNT;
}

void block_free()
{
    cpu_buffer_free(&cpu_blocks);
    gpu_buffer_free(&gpu_blocks);
}

SDL_GPUBuffer* block_get_buffer()
{
    return gpu_blocks.buffer;
}

bool block_is_opaque(block_t block)
{
    return BLOCKS[block].is_opaque;
}

bool block_is_sprite(block_t block)
{
    return BLOCKS[block].is_sprite;
}

bool block_is_solid(block_t block)
{
    return BLOCKS[block].is_solid;
}

bool block_has_occlusion(block_t block)
{
    return BLOCKS[block].has_occlusion;
}

int block_get_index(block_t block, direction_t direction)
{
    return BLOCKS[block].indices[direction];
}

bool block_is_light(block_t block)
{
    return BLOCKS[block].light.radius > 0;
}

light_t block_get_light(block_t block)
{
    return BLOCKS[block].light;
}
