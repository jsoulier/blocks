#include <SDL3/SDL.h>

#include "camera.h"
#include "sky.h"
#include "world.h"

static const float kOrtho = 100.0f;
static const float kPitch = -45.0f;
static const float kYaw = 45.0f;
static const float kY = 30.0f;

void sky_init(sky_t* sky)
{
    camera_init(&sky->camera, CAMERA_TYPE_ORTHO);
    sky->camera.ortho = kOrtho;
}

void sky_update(sky_t* sky, camera_t* camera, float dt)
{
    float x;
    float y;
    float z;
    camera_get_position(camera, &x, &y, &z);
    x = SDL_floor(x / CHUNK_WIDTH) * CHUNK_WIDTH;
    y = kY;
    z = SDL_floor(z / CHUNK_WIDTH) * CHUNK_WIDTH;
    camera_set_position(&sky->camera, x, y, z);
    camera_set_rotation(&sky->camera, kPitch, kYaw, 0.0f);
    camera_update(&sky->camera);
}
