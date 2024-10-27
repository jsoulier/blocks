#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "block.h"
#include "camera.h"
#include "helpers.h"

typedef struct
{
    void* buffer;
    uint32_t size;
    uint32_t capacity;
    tag_t tag;
    block_t blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
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

bool world_init(void* device);
void world_free();
void world_update();
void world_render(
    const camera_t* camera,
    void* commands,
    void* pass);
void world_move(
    const int32_t x,
    const int32_t y,
    const int32_t z);
group_t* world_get_group(
    const int32_t x,
    const int32_t z);
chunk_t* world_get_chunk(
    const int32_t x,
    const int32_t y,
    const int32_t z);
void world_get_group_neighbors(
    const int32_t x,
    const int32_t z,
    group_t* neighbors[DIRECTION_2]);
void world_get_chunk_neighbors(
    const int32_t x,
    const int32_t y,
    const int32_t z,
    chunk_t* neighbors[DIRECTION_3]);
bool world_in(
    const int32_t x,
    const int32_t y,
    const int32_t z);
bool world_on_border(
    const int32_t x,
    const int32_t y,
    const int32_t z);
block_t world_get_block(
    const int32_t x,
    const int32_t y,
    const int32_t z);
void world_set_block(
    const int32_t x,
    const int32_t y,
    const int32_t z,
    const block_t block);
