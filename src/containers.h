#pragma once

#include <stdbool.h>
#include <stdint.h>
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
    void** data;
    int width, height;
    int32_t x, y;
} grid_t;

void grid_init(
    grid_t* grid,
    const int width,
    const int height);
void grid_free(grid_t* grid);
void* grid_get(
    const grid_t* grid,
    const int x,
    const int y);
void* grid_get2(
    const grid_t* grid,
    const int32_t x,
    const int32_t y);
void grid_set(
    grid_t* grid,
    const int x,
    const int y,
    void* data);
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

/// @brief Move the grid and return the indices to moved pointers
/// @param grid The grid
/// @param x The x position
/// @param y The y position
/// @param size The number of indices
/// @return The indices to moved pointers (The caller should free)
int* grid_move(
    grid_t* grid,
    const int32_t x,
    const int32_t y,
    int* size);