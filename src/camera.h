#pragma once

#include <stdbool.h>

typedef struct
{
    float view[4][4];
    float proj[4][4];
    float matrix[4][4];
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    float width;
    float height;
    float fov;
    float near;
    float far;
    bool dirty;
}
camera_t;

void camera_init(camera_t* camera);
void camera_update(camera_t* camera);
void camera_viewport(
    camera_t* camera,
    const int width,
    const int height);
void camera_move(
    camera_t* camera,
    const float x,
    const float y,
    const float z);
void camera_rotate(
    camera_t* camera,
    const float pitch,
    const float yaw);
void camera_set_position(
    camera_t* camera,
    const float x,
    const float y,
    const float z);
void camera_get_position(
    const camera_t* camera,
    float* x,
    float* y,
    float* z);
void camera_set_rotation(
    camera_t* camera,
    const float pitch,
    const float yaw);
void camera_get_rotation(
    const camera_t* camera,
    float* pitch,
    float* yaw);
void camera_vector(
    const camera_t* camera,
    float* x,
    float* y,
    float* z);
bool camera_test(
    const camera_t* camera,
    const float x,
    const float y,
    const float z,
    const float a,
    const float b,
    const float c);