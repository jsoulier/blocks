#include <SDL3/SDL.h>

#include "camera.h"

#define DEGREES(rad) ((rad) * 180.0f / SDL_PI_F)
#define RADIANS(deg) ((deg) * SDL_PI_F / 180.0f)

static void MultiplyMatrices(float matrix[4][4], float lhs[4][4], float rhs[4][4])
{
    float c[4][4] = {0};
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            c[i][j] += lhs[0][j] * rhs[i][0];
            c[i][j] += lhs[1][j] * rhs[i][1];
            c[i][j] += lhs[2][j] * rhs[i][2];
            c[i][j] += lhs[3][j] * rhs[i][3];
        }
    }
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            matrix[i][j] = c[i][j];
        }
    }
}

static void Perspective(float matrix[4][4], float aspect, float fov, float near, float far)
{
    matrix[0][0] = (1.0f / SDL_tanf(fov / 2.0f)) / aspect;
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = 1.0f / SDL_tanf(fov / 2.0f);
    matrix[1][2] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][0] = 0.0f;
    matrix[2][1] = 0.0f;
    matrix[2][2] = -far / (far - near);
    matrix[2][3] = -1.0f;
    matrix[3][2] = -(near * far) / (far - near);
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

static void CalculateFrustum(float planes[6][4], float matrix[4][4])
{
    planes[0][0] = matrix[0][3] + matrix[0][0];
    planes[0][1] = matrix[1][3] + matrix[1][0];
    planes[0][2] = matrix[2][3] + matrix[2][0];
    planes[0][3] = matrix[3][3] + matrix[3][0];
    planes[1][0] = matrix[0][3] - matrix[0][0];
    planes[1][1] = matrix[1][3] - matrix[1][0];
    planes[1][2] = matrix[2][3] - matrix[2][0];
    planes[1][3] = matrix[3][3] - matrix[3][0];
    planes[2][0] = matrix[0][3] + matrix[0][1];
    planes[2][1] = matrix[1][3] + matrix[1][1];
    planes[2][2] = matrix[2][3] + matrix[2][1];
    planes[2][3] = matrix[3][3] + matrix[3][1];
    planes[3][0] = matrix[0][3] - matrix[0][1];
    planes[3][1] = matrix[1][3] - matrix[1][1];
    planes[3][2] = matrix[2][3] - matrix[2][1];
    planes[3][3] = matrix[3][3] - matrix[3][1];
    planes[4][0] = matrix[0][2];
    planes[4][1] = matrix[1][2];
    planes[4][2] = matrix[2][2];
    planes[4][3] = matrix[3][2];
    planes[5][0] = matrix[0][3] - matrix[0][2];
    planes[5][1] = matrix[1][3] - matrix[1][2];
    planes[5][2] = matrix[2][3] - matrix[2][2];
    planes[5][3] = matrix[3][3] - matrix[3][2];
    for (int plane_index = 0; plane_index < 6; ++plane_index)
    {
        float length = 0.0f;
        length += planes[plane_index][0] * planes[plane_index][0];
        length += planes[plane_index][1] * planes[plane_index][1];
        length += planes[plane_index][2] * planes[plane_index][2];
        length = SDL_sqrtf(length);
        if (length < SDL_FLT_EPSILON)
        {
            continue;
        }
        planes[plane_index][0] /= length;
        planes[plane_index][1] /= length;
        planes[plane_index][2] /= length;
        planes[plane_index][3] /= length;
    }
}

static void Rotate(float matrix[4][4], float axis_x, float axis_y, float axis_z, float angle)
{
    float sine = SDL_sinf(angle);
    float cosine = SDL_cosf(angle);
    float inverse_cosine = 1.0f - cosine;
    matrix[0][0] = inverse_cosine * axis_x * axis_x + cosine;
    matrix[0][1] = inverse_cosine * axis_x * axis_y - axis_z * sine;
    matrix[0][2] = inverse_cosine * axis_z * axis_x + axis_y * sine;
    matrix[0][3] = 0.0f;
    matrix[1][0] = inverse_cosine * axis_x * axis_y + axis_z * sine;
    matrix[1][1] = inverse_cosine * axis_y * axis_y + cosine;
    matrix[1][2] = inverse_cosine * axis_y * axis_z - axis_x * sine;
    matrix[1][3] = 0.0f;
    matrix[2][0] = inverse_cosine * axis_z * axis_x - axis_y * sine;
    matrix[2][1] = inverse_cosine * axis_y * axis_z + axis_x * sine;
    matrix[2][2] = inverse_cosine * axis_z * axis_z + cosine;
    matrix[2][3] = 0.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;
}

void Camera_Init(Camera* camera, CameraType type)
{
    camera->type = type;
    camera->x = 0.0f;
    camera->y = 0.0f;
    camera->z = 0.0f;
    camera->pitch = 0.0f;
    camera->yaw = 0.0f;
    camera->width = 1;
    camera->height = 1;
    camera->fov = RADIANS(90.0f);
    camera->near = 0.1f;
    camera->far = 1000.0f;
    camera->ortho = 100.0f;
}

void Camera_Update(Camera* camera)
{
    float sin_yaw = SDL_sinf(camera->yaw);
    float cos_yaw = SDL_cosf(camera->yaw);
    float rotation[4][4];
    Translate(camera->view, -camera->x, -camera->y, -camera->z);
    Rotate(rotation, cos_yaw, 0.0f, sin_yaw, camera->pitch);
    MultiplyMatrices(camera->view, rotation, camera->view);
    Rotate(rotation, 0.0f, 1.0f, 0.0f, -camera->yaw);
    MultiplyMatrices(camera->view, rotation, camera->view);
    float aspect = (float)camera->width / camera->height;
    if (camera->type == CAMERA_TYPE_PERSPECTIVE)
    {
        Perspective(camera->proj, aspect, camera->fov, camera->near, camera->far);
    }
    else
    {
        float ox = camera->ortho * aspect;
        float oy = camera->ortho;
        Ortho(camera->proj, -ox, ox, -oy, oy, -camera->far, camera->far);
    }
    MultiplyMatrices(camera->matrix, camera->proj, camera->view);
    CalculateFrustum(camera->planes, camera->matrix);
}

void Camera_Move(Camera* camera, float right, float up, float forward)
{
    float sin_yaw = SDL_sinf(camera->yaw);
    float cos_yaw = SDL_cosf(camera->yaw);
    float sin_pitch = SDL_sinf(camera->pitch);
    float cos_pitch = SDL_cosf(camera->pitch);
    camera->x += cos_pitch * sin_yaw * forward + cos_yaw * right;
    camera->y += up + forward * sin_pitch;
    camera->z -= cos_pitch * cos_yaw * forward - sin_yaw * right;
    camera->y = SDL_clamp(camera->y, -camera->far, camera->far);
}

void Camera_Resize(Camera* camera, int width, int height)
{
    SDL_assert(width > 0.0f);
    SDL_assert(height > 0.0f);
    camera->width = width;
    camera->height = height;
}

void Camera_Rotate(Camera* camera, float pitch, float yaw)
{
    static const float PITCH = SDL_PI_F / 2.0f - SDL_FLT_EPSILON;
    camera->pitch += RADIANS(pitch);
    camera->yaw += RADIANS(yaw);
    camera->pitch = SDL_clamp(camera->pitch, -PITCH, PITCH);
}

void Camera_GetVector(const Camera* camera, float* x, float* y, float* z)
{
    float cos_pitch = SDL_cosf(camera->pitch);
    *x = SDL_cosf(camera->yaw - RADIANS(90.0f)) * cos_pitch;
    *y = SDL_sinf(camera->pitch);
    *z = SDL_sinf(camera->yaw - RADIANS(90.0f)) * cos_pitch;
}

bool Camera_GetVisibility(const Camera* camera, float x, float y, float z, float width, float height, float depth)
{
    float max_x = x + width;
    float max_y = y + height;
    float max_z = z + depth;
    for (int plane_index = 0; plane_index < 6; ++plane_index)
    {
        const float* plane = camera->planes[plane_index];
        float test_x = plane[0] >= 0.0f ? max_x : x;
        float test_y = plane[1] >= 0.0f ? max_y : y;
        float test_z = plane[2] >= 0.0f ? max_z : z;
        if (plane[0] * test_x + plane[1] * test_y + plane[2] * test_z + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}
