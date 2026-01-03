#include <SDL3/SDL.h>

#include "sort.h"

static int Compare(void* userdata, const void* lhs, const void* rhs)
{
    const int* l = lhs;
    const int* r = rhs;
    int cx = ((int*) userdata)[0];
    int cy = ((int*) userdata)[1];
    int a = (l[0] - cx) * (l[0] - cx) + (l[1] - cx) * (l[1] - cx);
    int b = (r[0] - cy) * (r[0] - cy) + (l[1] - cy) * (l[1] - cy);
    if (a < b)
    {
        return -1;
    }
    else if (a > b)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void SortXY(int x, int y, int* data, int size)
{
    SDL_qsort_r(data, size * 2, sizeof(int), Compare, (int[]) {x, y});
}
