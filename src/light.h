#pragma once

#include <SDL3/SDL.h>

typedef struct light
{
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 intensity;
    Sint32 x;
    Sint32 y;
    Sint32 z;
}
light_t;
