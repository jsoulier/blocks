#pragma once

#include <SDL3/SDL.h>

typedef enum camera_type
{
    CAMERA_TYPE_ORTHO,
    CAMERA_TYPE_PERSPECTIVE,
    CAMERA_TYPE_COUNT,
}
camera_type_t;

typedef struct camera
{
    camera_type_t type;
    float view[4][4];
    float proj[4][4];
    float matrix[4][4];
    float planes[6][4];
    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };
        float position[3];
    };
    float pitch;
    float yaw;
    union
    {
        struct
        {
            Sint32 width;
            Sint32 height;
        };
        Sint32 size[2];
    };
    float fov;
    float near;
    float far;
    float ortho;
}
camera_t;

void camera_init(camera_t* camera, camera_type_t type);
void camera_update(camera_t* camera);
void camera_move(camera_t* camera, float x, float y, float z);
void camera_resize(camera_t* camera, int width, int height);
void camera_rotate(camera_t* camera, float pitch, float yaw);
void camera_get_vector(const camera_t* camera, float* x, float* y, float* z);
bool camera_get_vis(const camera_t* camera, float x, float y, float z, float sx, float sy, float sz);
