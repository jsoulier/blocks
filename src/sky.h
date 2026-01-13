// move to main

#pragma once

#include "camera.h"

typedef struct sky
{
    camera_t camera;
}
sky_t;

void sky_init(sky_t* sky);
void sky_update(sky_t* sky, camera_t* camera, float dt);
