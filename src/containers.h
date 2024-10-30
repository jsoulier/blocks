#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"

typedef struct {
    uint8_t* data;
    int size, stride;
    int head, tail;
} ring_t;

void ring_init(
    ring_t* ring,
    const int size,
    const int stride);
void ring_free(ring_t* ring);
bool ring_add(
    ring_t* ring,
    const void* item,
    const bool priority);
bool ring_remove(ring_t* ring, void* item);

typedef struct {
    tag_t tag;
    block_t blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
    struct {
        SDL_GPUBuffer* vbo;
        uint32_t size;
        uint32_t capacity;
    } opaque, transparent;
    bool renderable;
    bool empty;
} chunk_t;

int chunk_wrap_x(const int x);
int chunk_wrap_z(const int z);
bool chunk_in(
    const int32_t x,
    const int32_t y,
    const int32_t z);

typedef struct {
    tag_t tag;
    chunk_t chunks[GROUP_CHUNKS];
    direction_t neighbors;
    bool loaded;
} group_t;

block_t group_get_group(
    const group_t* group,
    const int x,
    const int y,
    const int z);
void group_set_block(
    group_t* group,
    const int x,
    const int y,
    const int z,
    const block_t block);

typedef struct {
    group_t* groups[WORLD_X][WORLD_Z];
    int32_t x, y;
} grid_t;

void grid_init(grid_t* grid);
void grid_free(grid_t* grid);
void* grid_get(
    const grid_t* grid,
    const int x,
    const int y);
void* grid_get2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y);
bool grid_in(
    const grid_t* grid,
    const int x,
    const int y);
bool grid_border(
    const grid_t* grid,
    const int x,
    const int y);
bool grid_in2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y);
bool grid_border2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y);
void grid_neighbors2(
    grid_t* grid,
    const int32_t x,
    const int32_t y,
    void* neighbors[DIRECTION_2]);
int* grid_move(
    grid_t* grid,
    const int32_t x,
    const int32_t y,
    int* size);