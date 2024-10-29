#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "containers.h"
#include "helpers.h"

void ring_init(
    ring_t* ring,
    const int size,
    const int stride)
{
    assert(ring);
    assert(size);
    assert(stride);
    ring->size = size + 1;
    ring->stride = stride;
    ring->head = 0;
    ring->tail = 0;
    ring->data = malloc(stride * ring->size);
    assert(ring->data);
}

void ring_free(ring_t* ring)
{
    assert(ring);
    free(ring->data);
    ring->data = NULL;
}

bool ring_add(
    ring_t* ring,
    const void* item,
    const bool priority)
{
    assert(ring);
    assert(ring->data);
    assert(item);
    if ((ring->tail + 1) % ring->size == ring->head) {
        return false;
    }
    if (priority) {
        ring->head = (ring->head - 1 + ring->size) % ring->size;
        memcpy(ring->data + ring->head * ring->stride, item, ring->stride);
    } else {
        memcpy(ring->data + ring->tail * ring->stride, item, ring->stride);
        ring->tail = (ring->tail + 1) % ring->size;
    }
    return true;
}

bool ring_remove(ring_t* ring, void* item)
{
    assert(ring);
    assert(ring->data);
    assert(item);
    if (ring->head == ring->tail) {
        return false;
    }
    memcpy(item, ring->data + ring->head * ring->stride, ring->stride);
    ring->head = (ring->head + 1) % ring->size;
    return true;
}

int chunk_wrap_x(const int x)
{
    return (x % CHUNK_X + CHUNK_X) % CHUNK_X;
}

int chunk_wrap_z(const int z)
{
    return (z % CHUNK_Z + CHUNK_Z) % CHUNK_Z;
}

bool chunk_in(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x >= 0 &&
        y >= 0 &&
        z >= 0 &&
        x < CHUNK_X &&
        y < CHUNK_Y &&
        z < CHUNK_Z;
}

block_t group_get_group(
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

void group_set_block(
    group_t* group,
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    assert(group);
    const int a = y / CHUNK_Y;
    const int b = y - a * CHUNK_Y;
    chunk_t* chunk = &group->chunks[a];
    chunk->blocks[x][b][z] = block;
    chunk->empty = false;
}

void grid_init(grid_t* grid)
{
    assert(grid);
    grid->x = INT32_MAX;
    grid->y = INT32_MAX;
    for (int x = 0; x < WORLD_X; x++) {
        for (int y = 0; y < WORLD_Z; y++) {
            grid->groups[x][y] = calloc(1, sizeof(group_t));
            assert(grid->groups[x][y]);
        }
    }
}

void grid_free(grid_t* grid)
{
    assert(grid);
    for (int x = 0; x < WORLD_X; x++) {
        for (int y = 0; y < WORLD_Z; y++) {
            free(grid->groups[x][y]);
            grid->groups[x][y] = NULL;
        }
    }
}

void* grid_get(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    assert(grid_in(grid, x, y));
    assert(grid->groups[x][y]);
    return grid->groups[x][y];
}

void* grid_get2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    return grid_get(grid, a, b);
}

bool grid_in(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    return
        x >= 0 &&
        y >= 0 &&
        x < WORLD_X &&
        y < WORLD_Z;
}

bool grid_border(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    return
        x == 0 ||
        y == 0 ||
        x == WORLD_X - 1 ||
        y == WORLD_Z - 1;
}

bool grid_in2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    return grid_in(grid, a, b);
}

bool grid_border2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    return grid_border(grid, a, b);
}

void grid_neighbors2(
    grid_t* grid,
    const int32_t x,
    const int32_t y,
    void* neighbors[DIRECTION_2])
{
    assert(grid);
    assert(grid_in2(grid, x, y));
    for (direction_t d = 0; d < DIRECTION_2; d++) {
        const int a = x + directions[d][0];
        const int b = y + directions[d][2];
        if (grid_in2(grid, a, b)) {
            neighbors[d] = grid_get2(grid, a, b);
        } else {
            neighbors[d] = NULL;
        }
    }
}

int* grid_move(
    grid_t* grid,
    const int32_t x,
    const int32_t y,
    int* size)
{
    assert(grid);
    assert(size);
    *size = 0;
    const int a = x - grid->x;
    const int b = y - grid->y;
    if (!a && !b) {
        return NULL;
    }
    grid->x = x;
    grid->y = y;
    group_t* groups1[WORLD_X][WORLD_Z] = {0};
    group_t* groups2[WORLD_GROUPS];
    int* indices = malloc(WORLD_GROUPS * 2 * sizeof(int));
    assert(indices);
    for (int i = 0; i < WORLD_X; i++) {
        for (int j = 0; j < WORLD_Z; j++) {
            const int c = i - a;
            const int d = j - b;
            if (grid_in(grid, c, d)) {
                groups1[c][d] = grid_get(grid, i, j);
            } else {
                groups2[(*size)++] = grid_get(grid, i, j);
            }
            grid->groups[i][j] = NULL;
        }
    }
    memcpy(grid->groups, groups1, sizeof(groups1));
    int n = *size;
    for (int i = 0; i < WORLD_X; i++) {
        for (int j = 0; j < WORLD_Z; j++) {
            if (grid->groups[i][j]) {
                continue;
            }
            --n;
            grid->groups[i][j] = groups2[n];
            indices[n * 2 + 0] = i;
            indices[n * 2 + 1] = j;
        }
    }
    assert(!n);
    return indices;
}