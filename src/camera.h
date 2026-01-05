#pragma once

#include <SDL3/SDL.h>

typedef enum CameraType
{
    CameraTypeOrtho,
    CameraTypePerspective,
    CameraTypeCount,
}
CameraType;

typedef struct Camera
{
    CameraType Type;
    float Matrix[4][4];
    float View[4][4];
    float Proj[4][4];
    float Planes[6][4];
    float X;
    float Y;
    float Z;
    float Pitch;
    float Yaw;
    float Roll;
    int Width;
    int Height;
    float Fov;
    float Near;
    float Far;
    float Ortho;
}
Camera;

void CreateCamera(Camera* camera, CameraType type);
void UpdateCamera(Camera* camera);
void SetCameraViewport(Camera* camera, int width, int height);
void MoveCamera(Camera* camera, float x, float y, float z);
void RotateCamera(Camera* camera, float pitch, float yaw);
void SetCameraPosition(Camera* camera, float x, float y, float z);
void GetCameraPosition(const Camera* camera, float* x, float* y, float* z);
void SetCameraRotation(Camera* camera, float pitch, float yaw, float roll);
void GetCameraRotation(const Camera* camera, float* pitch, float* yaw, float* roll);
void GetCameraVector(const Camera* camera, float* x, float* y, float* z);
bool GetCameraVisibility(const Camera* camera, float x, float y, float z, float a, float b, float c);
