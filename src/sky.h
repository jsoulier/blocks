#pragma once

#include <SDL3/SDL.h>

#include "camera.h"

typedef struct SkyRender
{
    float sun[4];
    float top[4];
    float horizon[4];
    float ambient[4];
}
SkyRender;

typedef struct Sky
{
    float time_of_day;
    bool has_sun;
    SkyRender render;
    Camera shadow_camera;
    int shadow_frame;
}
Sky;

void Sky_Load(Sky* sky);
void Sky_Reset(Sky* sky);
void Sky_Save(const Sky* sky);
void Sky_Update(Sky* sky, float dt);
void Sky_UpdateShadow(Sky* sky, const Camera* camera, int resolution);
