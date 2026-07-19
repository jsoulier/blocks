#pragma once

#include <SDL3/SDL.h>

typedef struct Sky
{
    float sun[4];
    float top[4];
    float horizon[4];
    float ambient[4];
    float time_of_day;
} Sky;

void Sky_Reset(Sky* sky);
void Sky_Save(const Sky* sky);
void Sky_Load(Sky* sky);
void Sky_Update(Sky* sky, float dt);
