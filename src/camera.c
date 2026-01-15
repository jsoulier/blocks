#include <SDL3/SDL.h>

#include "camera.h"
#include "check.h"

#define DEGREES(rad) ((rad) * 180.0f / SDL_PI_F)
#define RADIANS(deg) ((deg) * SDL_PI_F / 180.0f)

static void multiply(float matrix[4][4], float a[4][4], float b[4][4])
{
    // TODO: matrix layout
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

static void perspective(float matrix[4][4], float aspect, float fov, float near, float far)
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
    matrix[2][2] = -(far + near) / (far - near);
    matrix[2][3] = -1.0f;
    matrix[3][0] = 0.0f;
    matrix[3][1] = 0.0f;
    matrix[3][2] = -(2.0f * far * near) / (far - near);
    matrix[3][3] = 0.0f;
}

static void ortho(float matrix[4][4], float left, float right, float bottom, float top, float near, float far)
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

static void translate(float matrix[4][4], float x, float y, float z)
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

static void frustum(float planes[6][4], float a[4][4])
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

static void rotate(float matrix[4][4], float x, float y, float z, float angle)
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

void camera_init(camera_t* camera, camera_type_t type)
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

void camera_update(camera_t* camera)
{
    float s = SDL_sinf(camera->yaw);
    float c = SDL_cosf(camera->yaw);
    translate(camera->view, -camera->x, -camera->y, -camera->z);
    rotate(camera->proj, c, 0.0f, s, camera->pitch);
    multiply(camera->view, camera->proj, camera->view);
    rotate(camera->proj, 0.0f, 1.0f, 0.0f, -camera->yaw);
    multiply(camera->view, camera->proj, camera->view);
    float aspect = (float) camera->width / camera->height;
    if (camera->type == CAMERA_TYPE_PERSPECTIVE)
    {
        perspective(camera->proj, aspect, camera->fov, camera->near, camera->far);
    }
    else
    {
        float ox = camera->ortho * aspect;
        float oy = camera->ortho;
        ortho(camera->proj, -ox, ox, -oy, oy, -camera->far, camera->far);
    }
    multiply(camera->matrix, camera->proj, camera->view);
    frustum(camera->planes, camera->matrix);
}

void camera_move(camera_t* camera, float x, float y, float z)
{
    float sy = SDL_sinf(camera->yaw);
    float cy = SDL_cosf(camera->yaw);
    float sp = SDL_sinf(camera->pitch);
    float cp = SDL_cosf(camera->pitch);
    camera->x += cp * (sy * z) + cy * x;
    camera->y += y + z * sp;
    camera->z -= cp * (cy * z) - sy * x;
}

void camera_resize(camera_t* camera, int width, int height)
{
    CHECK(width > 0.0f);
    CHECK(height > 0.0f);
    camera->width = width;
    camera->height = height;
}

void camera_rotate(camera_t* camera, float pitch, float yaw)
{
    static const float PITCH = SDL_PI_F / 2.0f - SDL_FLT_EPSILON;
    camera->pitch += RADIANS(pitch);
    camera->yaw += RADIANS(yaw);
    camera->pitch = SDL_clamp(camera->pitch, -PITCH, PITCH);
}

void camera_get_vector(const camera_t* camera, float* x, float* y, float* z)
{
    float c = SDL_cosf(camera->pitch);
    *x = SDL_cosf(camera->yaw - RADIANS(90.0f)) * c;
    *y = SDL_sinf(camera->pitch);
    *z = SDL_sinf(camera->yaw - RADIANS(90.0f)) * c;
}

bool camera_get_vis(const camera_t* camera, float x, float y, float z, float sx, float sy, float sz)
{
    float x2 = x + sx;
    float y2 = y + sy;
    float z2 = z + sz;
    for (int i = 0; i < 6; ++i)
    {
        const float *plane = camera->planes[i];
        float a = plane[0] >= 0.0f ? x2 : x;
        float b = plane[1] >= 0.0f ? y2 : y;
        float c = plane[2] >= 0.0f ? z2 : z;
        if (plane[0] * a + plane[1] * b + plane[2] * c + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}
