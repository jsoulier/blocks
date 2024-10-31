#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "helpers.h"

typedef struct
{
    uint8_t* data;
    int size;
    int stride;
    int head;
    int tail;
}
queue_t;

void queue_init(
    queue_t* queue,
    const int size,
    const int stride);
void queue_free(queue_t* queue);
bool queue_add(
    queue_t* queue,
    const void* item,
    const bool priority);
bool queue_remove(queue_t* queue, void* item);

typedef struct
{
    tag_t tag;
    block_t blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
    SDL_GPUBuffer* opaque_vbo;
    SDL_GPUBuffer* transparent_vbo;
    uint32_t opaque_size;
    uint32_t transparent_size;
    uint32_t opaque_capacity;
    uint32_t transparent_capacity;
    bool renderable;
    bool empty;
}
chunk_t;

void chunk_wrap(
    int* x,
    int* y,
    int* z);
bool chunk_in(
    const int x,
    const int y,
    const int z);

typedef struct
{
    tag_t tag;
    chunk_t chunks[GROUP_CHUNKS];
    direction_t neighbors;
    bool loaded;
}
group_t;

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

typedef struct
{
    group_t* groups[WORLD_X][WORLD_Z];
    int x;
    int z;
}
terrain_t;

void terrain_init(terrain_t* terrain);
void terrain_free(terrain_t* terrain);
group_t* terrain_get(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_in(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_border(
    const terrain_t* terrain,
    const int x,
    const int z);
group_t* terrain_get2(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_in2(
    const terrain_t* terrain,
    const int x,
    const int z);
bool terrain_border2(
    const terrain_t* terrain,
    const int x,
    const int z);
void terrain_neighbors2(
    terrain_t* terrain,
    const int x,
    const int z,
    void* neighbors[DIRECTION_2]);
int* terrain_move(
    terrain_t* terrain,
    const int x,
    const int z,
    int* size);