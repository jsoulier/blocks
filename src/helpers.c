#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "helpers.h"

const int directions[][3] =
{
    { 0, 0, 1 },
    { 0, 0,-1 },
    { 1, 0, 0 },
    {-1, 0, 0 },
    { 0, 1, 0 },
    { 0,-1, 0 },
};

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
    if (is_2d) {
        c = dot(l[0], 0, l[1]);
        d = dot(r[0], 0, r[1]);
    } else {
        c = dot(l[0], l[1], l[2]);
        d = dot(r[0], r[1], r[2]);
    }
    if (c < d) {
        return is_ascending ? -1 : 1;
    } else if (c > d) {
        return is_ascending ? 1 : -1;
    } else {
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