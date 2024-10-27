#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "containers.h"
#include "helpers.h"

////////////////////////////////////////////////////////////////////////////////
// Ring Implementation /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// Grid Implementation /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void grid_init(
    grid_t* grid,
    const int width,
    const int height)
{
    assert(grid);
    assert(width);
    assert(height);
    grid->width = width;
    grid->height = height;
    grid->x = INT32_MAX;
    grid->y = INT32_MAX;
    grid->data = calloc(width * height, sizeof(grid->data));
    assert(grid->data);
}

void grid_free(grid_t* grid)
{
    assert(grid);
    assert(grid->data);
    for (int x = 0; x < grid->width; x++) {
        for (int y = 0; y < grid->height; y++) {
            free(grid_get(grid, x, y));
        }
    }
    free(grid->data);
    grid->data = NULL;
}

void* grid_get(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    assert(grid->data);
    assert(grid_in(grid, x, y));
    assert(grid->data[x * grid->width + y]);
    return grid->data[x * grid->width + y];
}

void* grid_get2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    assert(grid->data);
    assert(grid_in2(grid, x, y));
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    assert(grid->data[a * grid->width + b]);
    return grid->data[a * grid->width + b];
}

void grid_set(
    grid_t* grid,
    const int x,
    const int y,
    void* data)
{
    assert(grid);
    assert(grid->data);
    assert(grid_in(grid, x, y));
    assert(!grid->data[x * grid->width + y]);
    assert(data);
    grid->data[x * grid->width + y] = data;
}

bool grid_in(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    assert(grid->data);
    return
        x >= 0 &&
        y >= 0 &&
        x < grid->width &&
        y < grid->height;
}

bool grid_border(
    const grid_t* grid,
    const int x,
    const int y)
{
    assert(grid);
    assert(grid->data);
    return
        x == 0 ||
        y == 0 ||
        x == grid->width - 1 ||
        y == grid->height - 1;
}

bool grid_in2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    assert(grid->data);
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    return
        a >= 0 &&
        b >= 0 &&
        a < grid->width &&
        b < grid->height;
}

bool grid_border2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y)
{
    assert(grid);
    assert(grid->data);
    const int32_t a = x - grid->x;
    const int32_t b = y - grid->y;
    return
        a == 0 ||
        b == 0 ||
        a == grid->width - 1 ||
        b == grid->height - 1;
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
        if (!grid_in2(grid, a, b)) {
            neighbors[d] = NULL;
            continue;
        }
        neighbors[d] = grid_get2(grid, a, b);
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
    const int w = grid->width;
    const int h = grid->height;
    grid->x = x;
    grid->y = y;
    void** data1 = calloc(w * h, sizeof(grid->data));
    void** data2 = malloc(w * h * sizeof(grid->data));
    int* indices = malloc(w * h * 2 * sizeof(int));
    assert(data1 && data2);
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            const int c = i - a;
            const int d = j - b;
            if (grid_in(grid, c, d)) {
                data1[c * w + d] = grid_get(grid, i, j);
            } else {
                data2[(*size)++] = grid_get(grid, i, j);
            }
            grid->data[i * w + j] = NULL;
        }
    }
    memcpy(grid->data, data1, w * h * sizeof(grid->data));
    int n = *size;
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            if (grid->data[i * w + j]) {
                continue;
            }
            --n;
            grid->data[i * w + j] = data2[n];
            indices[n * 2 + 0] = i;
            indices[n * 2 + 1] = j;
        }
    }
    assert(!n);
    free(data1);
    free(data2);
    return indices;
}