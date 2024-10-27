#pragma once

#include <SDL3/SDL.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

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

typedef enum direction
{
    DIRECTION_N,
    DIRECTION_S,
    DIRECTION_E,
    DIRECTION_W,
    DIRECTION_U,
    DIRECTION_D,
    DIRECTION_2 = 4,
    DIRECTION_3 = 6,
} direction_t;

static const int directions[][3] =
{
    { 0, 0, 1 },
    { 0, 0,-1 },
    { 1, 0, 0 },
    {-1, 0, 0 },
    { 0, 1, 0 },
    { 0,-1, 0 },
};

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

typedef struct
{
    int a;
    int b;
} tag_t;

void tag_init(tag_t* tag);
void tag_invalidate(tag_t* tag);
bool tag_same(const tag_t a, const tag_t b);

#define CHUNK_X 30
#define CHUNK_Y 30
#define CHUNK_Z 30
#define GROUP_CHUNKS 9
#define GROUP_X (CHUNK_X)
#define GROUP_Y (CHUNK_Y * GROUP_CHUNKS)
#define GROUP_Z (CHUNK_Z)
#define WORLD_X 21
#define WORLD_Y (GROUP_CHUNKS)
#define WORLD_Z 21
#define WORLD_GROUPS (WORLD_X * WORLD_Z)
#define WORLD_CHUNKS (WORLD_Y * WORLD_GROUPS)

bool in_chunk(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool on_chunk_border(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool in_group(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool on_group_border(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool in_world(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool on_world_border(
    const int32_t x,
    const int32_t y,
    const int32_t z);

static int chunk_mod_x(int x)
{
    x = x % CHUNK_X;
    return x >= 0 ? x : x + CHUNK_X;
}

static int chunk_mod_z(int z)
{
    z = z % CHUNK_Z;
    return z >= 0 ? z : z + CHUNK_Z;
}

// TODO: move the types

typedef struct
{
    SDL_GPUBuffer* vbo;
    uint32_t size;
    uint32_t capacity;
    tag_t tag;
    // fix me to block_t
    uint8_t blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
    bool renderable;
    bool empty;
} chunk_t;

typedef struct
{
    tag_t tag;
    chunk_t chunks[GROUP_CHUNKS];
    direction_t neighbors;
    bool loaded;
} group_t;

static int get_chunk_block_from_group(
    const group_t* group,
    const int x,
    const int y,
    const int z)
{
    assert(group);
    const int a = y / CHUNK_Y;
    const int b = y - a * CHUNK_Y;
    const chunk_t* chunk = &group->chunks[a];
    assert(x < CHUNK_X);
    assert(z < CHUNK_Z);
    return chunk->blocks[x][b][z];
}

static void set_block_in_group(
    group_t* group,
    const int x,
    const int y,
    const int z,
    const int block)
{
    assert(group);
    const int a = y / CHUNK_Y;
    const int b = y - a * CHUNK_Y;
    chunk_t* chunk = &group->chunks[a];
    chunk->blocks[x][b][z] = block;
    chunk->empty = false;
}
