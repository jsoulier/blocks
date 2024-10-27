#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "helpers.h"

static thread_local int cx;
static thread_local int cy;
static thread_local int cz;
static thread_local bool is_ascending;
static thread_local bool is_2d;

static int dot(
    const int x,
    const int y,
    const int z)
{
    const int dx = x - cx;
    const int dy = y - cy;
    const int dz = z - cz;
    return dx * dx + dy * dy + dz * dz;
}

static int compare(const void* a, const void* b)
{
    const int* l = a;
    const int* r = b;
    int c;
    int d;
    if (is_2d)
    {
        c = dot(l[0], 0, l[1]);
        d = dot(r[0], 0, r[1]);
    }
    else
    {
        c = dot(l[0], l[1], l[2]);
        d = dot(r[0], r[1], r[2]);
    }
    if (c < d)
    {
        return is_ascending ? -1 : 1;
    }
    else if (c > d)
    {
        return is_ascending ? 1 : -1;
    }
    else
    {
        return 0;
    }
}

void sort_2d(
    const int x,
    const int z,
    void* data,
    const int size,
    const bool ascending)
{
    assert(data);
    assert(size);
    cx = x;
    cy = 0;
    cz = z;
    is_ascending = ascending;
    is_2d = true;
    qsort(data, size, 8, compare);
}

void sort_3d(
    const int x,
    const int y,
    const int z,
    void* data,
    const int size,
    const bool ascending)
{
    assert(data);
    assert(size);
    cx = x;
    cy = y;
    cz = z;
    is_ascending = ascending;
    is_2d = false;
    qsort(data, size, 12, compare);
}

static int counter = 0;

void tag_init(tag_t* tag)
{
    tag->a = counter++;
}

void tag_invalidate(tag_t* tag)
{
    tag->b++;
}

bool tag_same(const tag_t a, const tag_t b)
{
    return a.a == b.a && a.b == b.b;
}

static_assert(WORLD_X % 2 == 1, "");
static_assert(WORLD_Y % 2 == 1, "");
static_assert(WORLD_Z % 2 == 1, "");

bool in_chunk(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x >= 0 && x < CHUNK_X &&
        y >= 0 && y < CHUNK_Y &&
        z >= 0 && z < CHUNK_Z;
}

bool on_chunk_border(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x == 0 || x == CHUNK_X - 1 ||
        y == 0 || y == CHUNK_Y - 1 ||
        z == 0 || z == CHUNK_Z - 1;
}

bool in_group(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x >= 0 && x < GROUP_X &&
        y >= 0 && y < GROUP_Y &&
        z >= 0 && z < GROUP_Z;
}

bool on_group_border(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x == 0 || x == GROUP_X - 1 ||
        y == 0 || y == GROUP_Y - 1 ||
        z == 0 || z == GROUP_Z - 1;
}

bool in_world(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x >= 0 && x < WORLD_X &&
        y >= 0 && y < WORLD_Y &&
        z >= 0 && z < WORLD_Z;
}

bool on_world_border(
    const int32_t x,
    const int32_t y,
    const int32_t z)
{
    return
        x == 0 || x == WORLD_X - 1 ||
        y == 0 || y == WORLD_Y - 1 ||
        z == 0 || z == WORLD_Z - 1;
}