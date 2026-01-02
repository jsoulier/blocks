#include <SDL3/SDL.h>
#include <stdbool.h>
#include "camera.h"
#include "helpers.h"

static void multiply(
    float matrix[4][4],
    const float a[4][4],
    const float b[4][4])
{
    float c[4][4];
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            c[i][j] = 0.0f;
            c[i][j] += a[0][j] * b[i][0];
            c[i][j] += a[1][j] * b[i][1];
            c[i][j] += a[2][j] * b[i][2];
            c[i][j] += a[3][j] * b[i][3];
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

static void translate(
    float matrix[4][4],
    const float x,
    const float y,
    const float z)
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

static void rotate(
    float matrix[4][4],
    const float x,
    const float y,
    const float z,
    const float angle)
{
    const float s = SDL_sinf(angle);
    const float c = SDL_cosf(angle);
    const float i = 1.0f - c;
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

static void perspective(
    float matrix[4][4],
    const float aspect,
    const float fov,
    const float near,
    const float far)
{
    const float f = 1.0f / SDL_tanf(fov / 2.0f);
    matrix[0][0] = f / aspect;
    matrix[0][1] = 0.0f;
    matrix[0][2] = 0.0f;
    matrix[0][3] = 0.0f;
    matrix[1][0] = 0.0f;
    matrix[1][1] = f;
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

static void ortho(
    float matrix[4][4],
    const float left,
    const float right,
    const float bottom,
    const float top,
    const float near,
    const float far)
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

static void frustum(
    float planes[6][4],
    const float a[4][4])
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
        if (length < EPSILON)
        {
            continue;
        }
        planes[i][0] /= length;
        planes[i][1] /= length;
        planes[i][2] /= length;
        planes[i][3] /= length;
    }
}

void camera_init(
    camera_t* camera,
    const camera_type_t type)
{
    assert(camera);
    camera->type = type;
    camera->x = 0.0f;
    camera->y = 0.0f;
    camera->z = 0.0f;
    camera->pitch = rad(0.0f);
    camera->yaw = rad(0.0f);
    camera->width = 640.0f;
    camera->height = 480.0f;
    camera->fov = rad(90.0f);
    camera->near = 1.0f;
    camera->far = 300.0f;
    camera->ortho = 300.0f;
    camera->dirty = true;
}

void camera_update(
    camera_t* camera)
{
    assert(camera);
    if (!camera->dirty)
    {
        return;
    }
    const float s = SDL_sinf(camera->yaw);
    const float c = SDL_cosf(camera->yaw);
    translate(camera->view, -camera->x, -camera->y, -camera->z);
    rotate(camera->proj, c, 0.0f, s, camera->pitch);
    multiply(camera->view, camera->proj, camera->view);
    rotate(camera->proj, 0.0f, 1.0f, 0.0f, -camera->yaw);
    multiply(camera->view, camera->proj, camera->view);
    if (camera->type == CAMERA_TYPE_ORTHO)
    {
        const float o = camera->ortho;
        ortho(camera->proj, -o, o, -o, o, -camera->far, camera->far);
    }
    else
    {
        const float a = camera->width / camera->height;
        perspective(camera->proj, a, camera->fov, camera->near, camera->far);
    }
    multiply(camera->matrix, camera->proj, camera->view);
    frustum(camera->planes, camera->matrix);
    camera->dirty = false;
}

void camera_set_viewport(
    camera_t* camera,
    const int width,
    const int height)
{
    assert(camera);
    assert(width > 0.0f);
    assert(height > 0.0f);
    if (camera->width == width && camera->height == height)
    {
        return;
    }
    camera->width = width;
    camera->height = height;
    camera->dirty = true;
}

void camera_move(
    camera_t* camera,
    const float x,
    const float y,
    const float z)
{
    assert(camera);
    if (!x && !y && !z)
    {
        return;
    }
    const float s = SDL_sinf(camera->yaw);
    const float c = SDL_cosf(camera->yaw);
    const float a = SDL_sinf(camera->pitch);
    const float b = SDL_cosf(camera->pitch);
    camera->x += b * (s * z) + c * x;
    camera->y += y + z * a;
    camera->z -= b * (c * z) - s * x;
    camera->dirty = true;
}

void camera_rotate(
    camera_t* camera,
    const float pitch,
    const float yaw)
{
    assert(camera);
    if (!pitch && !yaw)
    {
        return;
    }
    const float a = camera->pitch + rad(pitch);
    const float b = camera->yaw + rad(yaw);
    camera_set_rotation(camera, a, b);
}

void camera_set_position(
    camera_t* camera,
    const float x,
    const float y,
    const float z)
{
    assert(camera);
    if (camera->x == x && camera->y == y && camera->z == z)
    {
        return;
    }
    camera->x = x;
    camera->y = y;
    camera->z = z;
    camera->dirty = true;
}

void camera_get_position(
    const camera_t* camera,
    float* x,
    float* y,
    float* z)
{
    assert(camera);
    assert(x);
    assert(y);
    assert(z);
    *x = camera->x;
    *y = camera->y;
    *z = camera->z;
}

void camera_set_rotation(
    camera_t* camera,
    const float pitch,
    const float yaw)
{
    assert(camera);
    if (camera->pitch == pitch && camera->yaw == yaw)
    {
        return;
    }
    const float e = PI / 2.0f - EPSILON;
    camera->pitch = clamp(pitch, -e, e);
    camera->yaw = yaw;
    camera->dirty = true;
}

void camera_get_rotation(
    const camera_t* camera,
    float* pitch,
    float* yaw)
{
    assert(camera);
    assert(pitch);
    assert(yaw);
    *pitch = camera->pitch;
    *yaw = camera->yaw;
}

void camera_get_vector(
    const camera_t* camera,
    float* x,
    float* y,
    float* z)
{
    assert(camera);
    assert(x);
    assert(y);
    assert(z);
    const float c = SDL_cosf(camera->pitch);
    *x = SDL_cosf(camera->yaw - rad(90)) * c;
    *y = SDL_sinf(camera->pitch);
    *z = SDL_sinf(camera->yaw - rad(90)) * c;
}

bool camera_test(
    const camera_t* camera,
    const float x,
    const float y,
    const float z,
    const float a,
    const float b,
    const float c)
{
    assert(camera);
    const float s = x + a;
    const float t = y + b;
    const float p = z + c;
    for (int i = 0; i < 6; ++i)
    {
        const float *plane = camera->planes[i];
        const float q = plane[0] >= 0.0f ? s : x;
        const float u = plane[1] >= 0.0f ? t : y;
        const float v = plane[2] >= 0.0f ? p : z;
        if (plane[0] * q + plane[1] * u + plane[2] * v + plane[3] < 0.0f)
        {
            return false;
        }
    }
    return true;
}