#pragma once

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#undef min
#undef max
#undef assert

#define EPSILON 0.000001
#define PI 3.14159265359
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define max3(a, b, c) (max(a, max(b, c)))
#define clamp(x, a, b) min(b, max(a, x))
#define deg(rad) ((rad) * 180.0 / PI)
#define rad(deg) ((deg) * PI / 180.0)

#ifndef NDEBUG
#define assert(e) SDL_assert_always(e)
#else
#define assert(e)
#endif

typedef enum {
    DIRECTION_N,
    DIRECTION_S,
    DIRECTION_E,
    DIRECTION_W,
    DIRECTION_U,
    DIRECTION_D,
    DIRECTION_2 = 4,
    DIRECTION_3 = 6,
} direction_t;

void sort_2d(
    const int x,
    const int z,
    void* data,
    const int size,
    const bool ascending);
void sort_3d(
    const int x,
    const int y,
    const int z,
    void* data,
    const int size,
    const bool ascending);

typedef struct {
    int a;
    int b;
} tag_t;

void tag_init(tag_t* tag);
void tag_invalidate(tag_t* tag);
bool tag_same(const tag_t a, const tag_t b);

typedef uint8_t block_t;
enum {
    BLOCK_EMPTY,
    BLOCK_CLOUD,
    BLOCK_DIRT,
    BLOCK_GLASS,
    BLOCK_GRASS,
    BLOCK_LAVA,
    BLOCK_SAND,
    BLOCK_SNOW,
    BLOCK_STONE,
    BLOCK_WATER,
    BLOCK_COUNT,
};

bool block_visible(const block_t a, const block_t b);

SDL_GPUShader* load_shader(
    SDL_GPUDevice* device,
    const char* file,
    const int uniforms,
    const int samplers);
SDL_Surface* load_bmp(const char* file);

extern const int directions[][3];
extern const int blocks[][DIRECTION_3][2];