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

/// @brief Test if an AABB can be seen from the camera
/// @param camera The camera
/// @param x The x position
/// @param y The y position
/// @param z The z position
/// @param a The x size
/// @param b The y size
/// @param c The z size
/// @return True if the AABB can be seen
bool camera_test(
    const camera_t* camera,
    const float x,
    const float y,
    const float z,
    const float a,
    const float b,
    const float c);