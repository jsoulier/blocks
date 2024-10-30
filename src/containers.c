#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "containers.h"
#include "helpers.h"

void queue_init(
    queue_t* queue,
    const int size,
    const int stride)
{
    assert(queue);
    assert(size);
    assert(stride);
    queue->size = size + 1;
    queue->stride = stride;
    queue->head = 0;
    queue->tail = 0;
    queue->data = malloc(stride * queue->size);
    assert(queue->data);
}

void queue_free(queue_t* queue)
{
    assert(queue);
    free(queue->data);
    queue->data = NULL;
}

bool queue_add(
    queue_t* queue,
    const void* item,
    const bool priority)
{
    assert(queue);
    assert(queue->data);
    assert(item);
    if ((queue->tail + 1) % queue->size == queue->head)
    {
        return false;
    }
    if (priority)
    {
        queue->head = (queue->head - 1 + queue->size) % queue->size;
        memcpy(queue->data + queue->head * queue->stride, item, queue->stride);
    }
    else
    {
        memcpy(queue->data + queue->tail * queue->stride, item, queue->stride);
        queue->tail = (queue->tail + 1) % queue->size;
    }
    return true;
}

bool queue_remove(queue_t* queue, void* item)
{
    assert(queue);
    assert(queue->data);
    assert(item);
    if (queue->head == queue->tail)
    {
        return false;
    }
    memcpy(item, queue->data + queue->head * queue->stride, queue->stride);
    queue->head = (queue->head + 1) % queue->size;
    return true;
}

void chunk_wrap(
    int* x,
    int* y,
    int* z)
{
    assert(x);
    assert(y);
    assert(z);
    *x = (*x % CHUNK_X + CHUNK_X) % CHUNK_X;
    *y = (*y % CHUNK_Y + CHUNK_Y) % CHUNK_Y;
    *z = (*z % CHUNK_Z + CHUNK_Z) % CHUNK_Z;
}

bool chunk_in(
    const int x,
    const int y,
    const int z)
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
    assert(a < GROUP_CHUNKS);
    const chunk_t* chunk = &group->chunks[a];
    assert(chunk_in(x, b, z));
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
    assert(a < GROUP_CHUNKS);
    chunk_t* chunk = &group->chunks[a];
    assert(chunk_in(x, b, z));
    chunk->blocks[x][b][z] = block;
    chunk->empty = false;
}

void terrain_init(terrain_t* terrain)
{
    assert(terrain);
    terrain->x = INT_MAX;
    terrain->z = INT_MAX;
    for (int x = 0; x < WORLD_X; x++)
    {
        for (int z = 0; z < WORLD_Z; z++)
        {
            terrain->groups[x][z] = calloc(1, sizeof(group_t));
            assert(terrain->groups[x][z]);
        }
    }
}

void terrain_free(terrain_t* terrain)
{
    assert(terrain);
    for (int x = 0; x < WORLD_X; x++)
    {
        for (int z = 0; z < WORLD_Z; z++)
        {
            free(terrain->groups[x][z]);
            terrain->groups[x][z] = NULL;
        }
    }
}

group_t* terrain_get(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    assert(terrain_in(terrain, x, z));
    assert(terrain->groups[x][z]);
    return terrain->groups[x][z];
}

bool terrain_in(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    return
        x >= 0 &&
        z >= 0 &&
        x < WORLD_X &&
        z < WORLD_Z;
}

bool terrain_border(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    return
        x == 0 ||
        z == 0 ||
        x == WORLD_X - 1 ||
        z == WORLD_Z - 1;
}

group_t* terrain_get2(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    return terrain_get(terrain, a, b);
}

bool terrain_in2(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    return terrain_in(terrain, a, b);
}

bool terrain_border2(
    const terrain_t* terrain,
    const int x,
    const int z)
{
    assert(terrain);
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    return terrain_border(terrain, a, b);
}

void terrain_neighbors2(
    terrain_t* terrain,
    const int x,
    const int z,
    void* neighbors[DIRECTION_2])
{
    assert(terrain);
    assert(terrain_in2(terrain, x, z));
    for (direction_t d = 0; d < DIRECTION_2; d++)
    {
        const int a = x + directions[d][0];
        const int b = z + directions[d][2];
        if (terrain_in2(terrain, a, b))
        {
            neighbors[d] = terrain_get2(terrain, a, b);
        }
        else
        {
            neighbors[d] = NULL;
        }
    }
}

int* terrain_move(
    terrain_t* terrain,
    const int x,
    const int z,
    int* size)
{
    assert(terrain);
    assert(size);
    *size = 0;
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    if (!a && !b)
    {
        return NULL;
    }
    terrain->x = x;
    terrain->z = z;
    group_t* groups1[WORLD_X][WORLD_Z] = {0};
    group_t* groups2[WORLD_GROUPS];
    int* indices = malloc(WORLD_GROUPS * 2 * sizeof(int));
    assert(indices);
    for (int i = 0; i < WORLD_X; i++)
    {
        for (int j = 0; j < WORLD_Z; j++)
        {
            const int c = i - a;
            const int d = j - b;
            if (terrain_in(terrain, c, d))
            {
                groups1[c][d] = terrain_get(terrain, i, j);
            }
            else
            {
                groups2[(*size)++] = terrain_get(terrain, i, j);
            }
            terrain->groups[i][j] = NULL;
        }
    }
    memcpy(terrain->groups, groups1, sizeof(groups1));
    int n = *size;
    for (int i = 0; i < WORLD_X; i++)
    {
        for (int j = 0; j < WORLD_Z; j++)
        {
            if (terrain->groups[i][j])
            {
                continue;
            }
            --n;
            terrain->groups[i][j] = groups2[n];
            indices[n * 2 + 0] = i;
            indices[n * 2 + 1] = j;
        }
    }
    assert(!n);
    return indices;
}