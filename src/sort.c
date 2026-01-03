#include <SDL3/SDL.h>

#include "sort.h"

static int squared(int* data, const int x, const int y)
{
    const int dx = x - data[0];
    const int dy = y - data[1];
    return dx * dx + dy * dy;
}

static int compare(void* data, const void* a, const void* b)
{
    const int* l = a;
    const int* r = b;
    const int c = squared(data, l[0], l[1]);
    const int d = squared(data, r[0], r[1]);
    if (c < d)
    {
        return -1;
    }
    else if (c > d)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void sort_xy(int x, int y, int* data, int size)
{
    SDL_qsort_r(data, size * 2, sizeof(int), compare, (int[]) {x, y});
}
