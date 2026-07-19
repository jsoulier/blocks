#include <SDL3/SDL.h>

#include "sort.h"

static int Distance2D(void* userdata, const void* lhs, const void* rhs)
{
    int center = *(int*) userdata;
    const int* l = lhs;
    const int* r = rhs;
    int dl = (l[0] - center) * (l[0] - center) + (l[1] - center) * (l[1] - center);
    int dr = (r[0] - center) * (r[0] - center) + (r[1] - center) * (r[1] - center);
    return (dl > dr) - (dl < dr);
}

void Sort_Distance2D(int data[][2], int size, int center)
{
    SDL_qsort_r(data, size, sizeof(data[0]), Distance2D, &center);
}
