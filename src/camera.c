#include <SDL3/SDL.h>

#include "camera.h"

#define Degrees(rad) ((rad) * 180.0f / SDL_PI_F)
#define Radians(deg) ((deg) * SDL_PI_F / 180.0f)

static const float kMaxPitch = SDL_PI_F / 2.0f - SDL_FLT_EPSILON;

static void Multiply(float matrix[4][4], float a[4][4], float b[4][4])
{
    // TODO: HLSL matrix layout
    float c[4][4];
    for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
        c[i][j] = 0.0f;
        c[i][j] += a[0][j] * b[i][0];
        c[i][j] += a[1][j] * b[i][1];
        c[i][j] += a[2][j] * b[i][2];
        c[i][j] += a[3][j] * b[i][3];
    }
    for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
        matrix[i][j] = c[i][j];
    }
}

static void Perspective(float matrix[4][4], float aspect, float fov, float near, float far)
{
    matrix[0][0] = (1.0f / SDL_tanf(fov / 2.0f)) / aspect;
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = (1.0f / SDL_tanf(fov / 2.0f));
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = -(far + near) / (far - near);
    matrix[2][3] = -1.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = -(2.0f * far * near) / (far - near);
    matrix[3][3] = 0.0f;
}

static void Ortho(float matrix[4][4], float left, float right, float bottom, float top, float near, float far)
{
    matrix[0][0] = 2.0f / (right - left);
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 2.0f / (top - bottom);
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = -1.0f / (far - near);
    matrix[2][3] = 0.0f;
    matrix[3][0] = -(right + left) / (right - left);
    matrix[3][1] = -(top + bottom) / (top - bottom);
    matrix[3][2] = -near / (far - near);
    matrix[3][3] = 1.0f;
}

static void Translate(float matrix[4][4], float x, float y, float z)
{
    matrix[0][0] = 1.0f;
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 1.0f;
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = 1.0f;
    matrix[2][3] = 0.0f;
    matrix[3][0] = x;
    matrix[3][1] = y;
    matrix[3][2] = z;
    matrix[3][3] = 1.0f;
}

static void Frustum(float planes[6][4], float a[4][4])
{
    planes[0][0] = a[0][3] + a[0][0];
    planes[0][1] = a[1][3] + a[1][0];
    planes[0][2] = a[2][3] + a[2][0];
    planes[0][3] = a[3][3] + a[3][0];
    planes[1][0] = a[0][3] - a[0][0];
    planes[1][1] = a[1][3] - a[1][0];
    planes[1][2] = a[2][3] - a[2][0];
    planes[1][3] = a[3][3] - a[3][0];
    planes[2][0] = a[0][3] + a[0][1];
    planes[2][1] = a[1][3] + a[1][1];
    planes[2][2] = a[2][3] + a[2][1];
    planes[2][3] = a[3][3] + a[3][1];
    planes[3][0] = a[0][3] - a[0][1];
    planes[3][1] = a[1][3] - a[1][1];
    planes[3][2] = a[2][3] - a[2][1];
    planes[3][3] = a[3][3] - a[3][1];
    planes[4][0] = a[0][3] + a[0][2];
    planes[4][1] = a[1][3] + a[1][2];
    planes[4][2] = a[2][3] + a[2][2];
    planes[4][3] = a[3][3] + a[3][2];
    planes[5][0] = a[0][3] - a[0][2];
    planes[5][1] = a[1][3] - a[1][2];
    planes[5][2] = a[2][3] - a[2][2];
    planes[5][3] = a[3][3] - a[3][2];
    for (int i = 0; i < 6; ++i)
    {
        float length = 0.0f;
        length += planes[i][0] * planes[i][0];
        length += planes[i][1] * planes[i][1];
        length += planes[i][2] * planes[i][2];
        length = SDL_sqrtf(length);
        if (length < SDL_FLT_EPSILON)
        {
            continue;
        }
        planes[i][0] /= length;
        planes[i][1] /= length;
        planes[i][2] /= length;
        planes[i][3] /= length;
    }
}

static void Rotate(float matrix[4][4], float x, float y, float z, float angle)
{
    float s = SDL_sinf(angle);
    float c = SDL_cosf(angle);
    float i = 1.0f - c;
    matrix[0][0] = i * x * x + c;
    matrix[0][1] = i * x * y - z * s;
    matrix[0][2] = i * z * x + y * s;
    matrix[0][3] = 0.0f;
    matrix[1][0] = i * x * y + z * s;
    matrix[1][1] = i * y * y + c;
    matrix[1][2] = i * y * z - x * s;
    matrix[1][3] = 0.0f;
    matrix[2][0] = i * z * x - y * s;
    matrix[2][1] = i * y * z + x * s;
    matrix[2][2] = i * z * z + c;
    matrix[2][3] = 0.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
}

void CreateCamera(Camera* camera, CameraType type)
{
    camera->Type = type;
    camera->X = 0.0f;
    camera->Y = 0.0f;
    camera->Z = 0.0f;
    camera->Pitch = 0.0f;
    camera->Yaw = 0.0f;
    camera->Roll = 0.0f;
    camera->Width = 1;
    camera->Height = 1;
    camera->Fov = Radians(90.0f);
    camera->Near = 0.1f;
    camera->Far = 1000.0f;
    camera->Ortho = 100.0f;
}

void UpdateCamera(Camera* camera)
{
    float s = SDL_sinf(camera->Yaw);
    float c = SDL_cosf(camera->Yaw);
    Translate(camera->View, -camera->X, -camera->Y, -camera->Z);
    Rotate(camera->Proj, c, 0.0f, s, camera->Pitch);
    Multiply(camera->View, camera->Proj, camera->View);
    Rotate(camera->Proj, 0.0f, 1.0f, 0.0f, -camera->Yaw);
    Multiply(camera->View, camera->Proj, camera->View);
    float aspect = (float) camera->Width / camera->Height;
    if (camera->Type == CameraTypeOrtho)
    {
        float ox = camera->Ortho * aspect;
        float oy = camera->Ortho;
        Ortho(camera->Proj, -ox, ox, -oy, oy, -camera->Far, camera->Far);
    }
    else
    {
        Perspective(camera->Proj, aspect, camera->Fov, camera->Near, camera->Far);
    }
    Multiply(camera->Matrix, camera->Proj, camera->View);
    Frustum(camera->Planes, camera->Matrix);
}

void SetCameraViewport(Camera* camera, int width, int height)
{
    SDL_assert(width > 0.0f);
    SDL_assert(height > 0.0f);
    camera->Width = width;
    camera->Height = height;
}

void MoveCamera(Camera* camera, float x, float y, float z)
{
    float s = SDL_sinf(camera->Yaw);
    float c = SDL_cosf(camera->Yaw);
    float a = SDL_sinf(camera->Pitch);
    float b = SDL_cosf(camera->Pitch);
    camera->X += b * (s * z) + c * x;
    camera->Y += y + z * a;
    camera->Z -= b * (c * z) - s * x;
}

void RotateCamera(Camera* camera, float pitch, float yaw)
{
    float a = camera->Pitch + Radians(pitch);
    float b = camera->Yaw + Radians(yaw);
    SetCameraRotation(camera, a, b, 0.0f);
}

void SetCameraPosition(Camera* camera, float x, float y, float z)
{
    camera->X = x;
    camera->Y = y;
    camera->Z = z;
}

void GetCameraPosition(const Camera* camera, float* x, float* y, float* z)
{
    *x = camera->X;
    *y = camera->Y;
    *z = camera->Z;
}

void SetCameraRotation(Camera* camera, float pitch, float yaw, float roll)
{
    camera->Pitch = SDL_clamp(pitch, -kMaxPitch, kMaxPitch);
    camera->Yaw = yaw;
    camera->Roll = roll;
}

void GetCameraRotation(const Camera* camera, float* pitch, float* yaw, float* roll)
{
    *pitch = camera->Pitch;
    *yaw = camera->Yaw;
    *roll = camera->Roll;
}

void GetCameraVector(const Camera* camera, float* x, float* y, float* z)
{
    *x = SDL_cosf(camera->Yaw - Radians(90.0f)) * SDL_cosf(camera->Pitch);
    *y = SDL_sinf(camera->Pitch);
    *z = SDL_sinf(camera->Yaw - Radians(90.0f)) * SDL_cosf(camera->Pitch);
}

bool GetCameraVisibility(const Camera* camera, float x, float y, float z, float a, float b, float c)
{
    float s = x + a;
    float t = y + b;
    float p = z + c;
    for (int i = 0; i < 6; ++i)
    {
        const float *plane = camera->Planes[i];
        float q = plane[0] >= 0.0f ? s : x;
        float u = plane[1] >= 0.0f ? t : y;
        float v = plane[2] >= 0.0f ? p : z;
        if (plane[0] * q + plane[1] * u + plane[2] * v + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}