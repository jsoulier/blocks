#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "chunk.h"
#include "helpers.h"

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

block_t group_get_block(
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

void terrain_init(
    terrain_t* terrain)
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

void terrain_free(
    terrain_t* terrain)
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

void terrain_neighbors(
    terrain_t* terrain,
    const int x,
    const int z,
    group_t* neighbors[DIRECTION_2])
{
    assert(terrain);
    assert(terrain_in(terrain, x, z));
    for (direction_t d = 0; d < DIRECTION_2; d++)
    {
        const int a = x + directions[d][0];
        const int b = z + directions[d][2];
        if (terrain_in(terrain, a, b))
        {
            neighbors[d] = terrain_get(terrain, a, b);
        }
        else
        {
            neighbors[d] = NULL;
        }
    }
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
    group_t* neighbors[DIRECTION_2])
{
    assert(terrain);
    const int a = x - terrain->x;
    const int b = z - terrain->z;
    terrain_neighbors(terrain, a, b, neighbors);
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
    group_t* in[WORLD_X][WORLD_Z] = {0};
    group_t* out[WORLD_GROUPS];
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
                in[c][d] = terrain_get(terrain, i, j);
            }
            else
            {
                out[(*size)++] = terrain_get(terrain, i, j);
            }
            terrain->groups[i][j] = NULL;
        }
    }
    memcpy(terrain->groups, in, sizeof(in));
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
            terrain->groups[i][j] = out[n];
            indices[n * 2 + 0] = i;
            indices[n * 2 + 1] = j;
        }
    }
    assert(!n);
    return indices;
}