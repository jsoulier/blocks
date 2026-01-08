#include <SDL3/SDL.h>

#include "sort.h"

static int compare(void* userdata, const void* lhs, const void* rhs)
{
    int cx = ((int*) userdata)[0];
    int cy = ((int*) userdata)[1];
    const int* l = lhs;
    const int* r = rhs;
    int a = (l[0] - cx) * (l[0] - cx) + (l[1] - cy) * (l[1] - cy);
    int b = (r[0] - cx) * (r[0] - cx) + (r[1] - cy) * (r[1] - cy);
    return (a > b) - (a < b);
}

void sort_xy(int x, int y, int* data, int size)
{
    SDL_qsort_r(data, size, sizeof(int) * 2, compare, (int[]) {x, y});
}
