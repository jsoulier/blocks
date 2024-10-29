#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "helpers.h"

static thread_local int cx;
static thread_local int cy;
static thread_local int cz;
static thread_local bool is_ascending;
static thread_local bool is_2d;

static int dot(
    const int x,
    const int y,
    const int z)
{
    const int dx = x - cx;
    const int dy = y - cy;
    const int dz = z - cz;
    return dx * dx + dy * dy + dz * dz;
}

static int compare(const void* a, const void* b)
{
    const int* l = a;
    const int* r = b;
    int c;
    int d;
    if (is_2d) {
        c = dot(l[0], 0, l[1]);
        d = dot(r[0], 0, r[1]);
    } else {
        c = dot(l[0], l[1], l[2]);
        d = dot(r[0], r[1], r[2]);
    }
    if (c < d) {
        return is_ascending ? -1 : 1;
    } else if (c > d) {
        return is_ascending ? 1 : -1;
    } else {
        return 0;
    }
}

void sort_2d(
    const int x,
    const int z,
    void* data,
    const int size,
    const bool ascending)
{
    assert(data);
    assert(size);
    cx = x;
    cy = 0;
    cz = z;
    is_ascending = ascending;
    is_2d = true;
    qsort(data, size, 8, compare);
}

void sort_3d(
    const int x,
    const int y,
    const int z,
    void* data,
    const int size,
    const bool ascending)
{
    assert(data);
    assert(size);
    cx = x;
    cy = y;
    cz = z;
    is_ascending = ascending;
    is_2d = false;
    qsort(data, size, 12, compare);
}

static bool opaque(const block_t block)
{
    assert(block > BLOCK_EMPTY);
    assert(block < BLOCK_COUNT);
    switch (block) {
    case BLOCK_GLASS:
    case BLOCK_WATER:
        return 0;
    }
    return 1;
}

bool block_visible(const block_t a, const block_t b)
{
    return b == BLOCK_EMPTY || (opaque(a) && !opaque(b));
}

static int counter = 0;

void tag_init(tag_t* tag)
{
    tag->a = counter++;
}

void tag_invalidate(tag_t* tag)
{
    tag->b++;
}

bool tag_same(const tag_t a, const tag_t b)
{
    return a.a == b.a && a.b == b.b;
}

SDL_GPUShader* load_shader(
    SDL_GPUDevice* device,
    const char* file,
    const int uniforms,
    const int samplers)
{
    assert(device);
    assert(file);
    SDL_GPUShaderCreateInfo info = {0};
    void* code = SDL_LoadFile(file, &info.code_size);
    if (!code) {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert")) {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.entrypoint = "main";
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

SDL_Surface* load_bmp(const char* file)
{
    SDL_Surface* argb32 = SDL_LoadBMP(file);
    if (!argb32) {
        SDL_Log("Failed to load %s: %s", file, SDL_GetError());
        return NULL;
    }
    SDL_Surface* rgba32 = SDL_ConvertSurface(argb32, SDL_PIXELFORMAT_RGBA32);
    if (!rgba32) {
        SDL_Log("Failed to convert %s: %s", file, SDL_GetError());
        return NULL;
    }
    SDL_DestroySurface(argb32);
    return rgba32;
}

const int directions[][3] =
{
    { 0, 0, 1 },
    { 0, 0,-1 },
    { 1, 0, 0 },
    {-1, 0, 0 },
    { 0, 1, 0 },
    { 0,-1, 0 },
};

const int blocks[][DIRECTION_3][2] =
{
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}, {3, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{2, 0}, {2, 0}, {2, 0}, {2, 0}, {1, 0}, {3, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
    {{4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}, {4, 0}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};