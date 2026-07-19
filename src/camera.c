#include <SDL3/SDL.h>

#include "camera.h"

#define DEGREES(rad) ((rad) * 180.0f / SDL_PI_F)
#define RADIANS(deg) ((deg) * SDL_PI_F / 180.0f)

static void Multiply(float matrix[4][4], float lhs[4][4], float rhs[4][4])
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

static void Frustum(float planes[6][4], float matrix[4][4])
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
    for (int i = 0; i < 6; i++)
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
    float sy = SDL_sinf(camera->yaw);
    float cy = SDL_cosf(camera->yaw);
    float rotation[4][4];
    Translate(camera->view, -camera->x, -camera->y, -camera->z);
    Rotate(rotation, cy, 0.0f, sy, camera->pitch);
    Multiply(camera->view, rotation, camera->view);
    Rotate(rotation, 0.0f, 1.0f, 0.0f, -camera->yaw);
    Multiply(camera->view, rotation, camera->view);
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
    Multiply(camera->matrix, camera->proj, camera->view);
    Frustum(camera->planes, camera->matrix);
}

void Camera_Snap(Camera* camera, int resolution)
{
    SDL_assert(camera->type == CAMERA_TYPE_ORTHO);
    SDL_assert(resolution > 0);
    Camera_Update(camera);
    float texel = camera->ortho * 2.0f / resolution;
    float rx = camera->view[0][0];
    float ry = camera->view[1][0];
    float rz = camera->view[2][0];
    float ux = camera->view[0][1];
    float uy = camera->view[1][1];
    float uz = camera->view[2][1];
    float x = rx * camera->x + ry * camera->y + rz * camera->z;
    float y = ux * camera->x + uy * camera->y + uz * camera->z;
    float dx = SDL_roundf(x / texel) * texel - x;
    float dy = SDL_roundf(y / texel) * texel - y;
    camera->x += rx * dx + ux * dy;
    camera->y += ry * dx + uy * dy;
    camera->z += rz * dx + uz * dy;
    Camera_Update(camera);
}

void Camera_Move(Camera* camera, float right, float up, float forward)
{
    float sy = SDL_sinf(camera->yaw);
    float cy = SDL_cosf(camera->yaw);
    float sp = SDL_sinf(camera->pitch);
    float cp = SDL_cosf(camera->pitch);
    camera->x += cp * sy * forward + cy * right;
    camera->y += up + forward * sp;
    camera->z -= cp * cy * forward - sy * right;
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
    float cp = SDL_cosf(camera->pitch);
    *x = SDL_cosf(camera->yaw - RADIANS(90.0f)) * cp;
    *y = SDL_sinf(camera->pitch);
    *z = SDL_sinf(camera->yaw - RADIANS(90.0f)) * cp;
}

bool Camera_IsVisible(const Camera* camera, float x, float y, float z, float width, float height, float depth)
{
    float max_x = x + width;
    float max_y = y + height;
    float max_z = z + depth;
    for (int i = 0; i < 6; i++)
    {
        const float* plane = camera->planes[i];
        float point_x = plane[0] >= 0.0f ? max_x : x;
        float point_y = plane[1] >= 0.0f ? max_y : y;
        float point_z = plane[2] >= 0.0f ? max_z : z;
        if (plane[0] * point_x + plane[1] * point_y + plane[2] * point_z + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}
