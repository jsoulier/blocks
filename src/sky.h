#pragma once

#include <SDL3/SDL.h>

#include "camera.h"

typedef struct Sky
{
    float sun[4];
    float top[4];
    float horizon[4];
    float ambient[4];
    float time_of_day;
    bool is_shadow_on;
    Camera camera;
    int frame;
} Sky;

void Sky_Reset(Sky* sky);
void Sky_Save(const Sky* sky);
void Sky_Load(Sky* sky);
void Sky_Update(Sky* sky, const Camera* camera, int resolution, float dt);
