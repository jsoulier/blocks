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
    float matrix[4][4];
    float view[4][4];
    float proj[4][4];
    float planes[6][4];

    // TODO: let's do a union here to make these more convenient to use
    float x;
    float y;
    float z;

    // TODO: same here
    float pitch;
    float yaw;
    float roll;

    int width;
    int height;
    float fov;
    float near;
    float far;
    float ortho;
}
camera_t;

void camera_init(camera_t* camera, camera_type_t type);
void camera_update(camera_t* camera);
void camera_set_viewport(camera_t* camera, int width, int height);
void camera_move(camera_t* camera, float x, float y, float z);
void camera_rotate(camera_t* camera, float pitch, float yaw);
void camera_set_position(camera_t* camera, float x, float y, float z);
void camera_get_position(const camera_t* camera, float* x, float* y, float* z);
void camera_set_rotation(camera_t* camera, float pitch, float yaw, float roll);
void camera_get_rotation(const camera_t* camera, float* pitch, float* yaw, float* roll);
void camera_get_vector(const camera_t* camera, float* x, float* y, float* z);
bool camera_get_visibility(const camera_t* camera, float x, float y, float z, float a, float b, float c);
