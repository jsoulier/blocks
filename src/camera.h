#pragma once

#include <SDL3/SDL.h>

typedef enum CameraType
{
    CAMERA_TYPE_ORTHO,
    CAMERA_TYPE_PERSPECTIVE,
} CameraType;

typedef struct Camera
{
    CameraType type;
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
    Sint32 width;
    Sint32 height;
    float fov;
    float near;
    float far;
    float ortho;
} Camera;

void Camera_Init(Camera* camera, CameraType type);
void Camera_Update(Camera* camera);
void Camera_Move(Camera* camera, float x, float y, float z);
void Camera_Resize(Camera* camera, int width, int height);
void Camera_Rotate(Camera* camera, float pitch, float yaw);
void Camera_GetVector(const Camera* camera, float* x, float* y, float* z);
bool Camera_GetVisibility(
    const Camera* camera,
    float x,
    float y,
    float z,
    float sx,
    float sy,
    float sz);
