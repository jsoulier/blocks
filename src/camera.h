#pragma once

#include <stdbool.h>

typedef struct {
    float matrix[4][4];
    float x, y, z;
    float pitch, yaw;
    float width, height;
    float fov;
    float near, far;
    bool dirty;
} camera_t;

void camera_init(camera_t* camera);
void camera_update(camera_t* camera);
void camera_move(
    camera_t* camera,
    const float x,
    const float y,
    const float z);
void camera_rotate(
    camera_t* camera,
    const float pitch,
    const float yaw);
void camera_size(
    camera_t* camera,
    const int width,
    const int height);
void camera_position(
    const camera_t* camera,
    float* x,
    float* y,
    float* z);
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