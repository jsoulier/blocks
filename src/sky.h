#pragma once

#include <SDL3/SDL.h>

#include "camera.h"

typedef struct sky_render
{
    float sun[4];
    float top[4];
    float horizon[4];
    float ambient[4];
}
sky_render_t;

typedef struct sky
{
    float time_of_day;
    bool has_sun;
    sky_render_t render;
    camera_t shadow_camera;
    int shadow_frame;
}
sky_t;

void sky_save_or_load(sky_t* sky, bool save);
void sky_update(sky_t* sky, float dt);
void sky_update_shadow(sky_t* sky, const camera_t* camera, int resolution);
