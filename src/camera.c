#include <math.h>
#include <stdbool.h>
#include "camera.h"
#include "helpers.h"

////////////////////////////////////////////////////////////////////////////////
// Matrix Helpers //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void multiply(
    float matrix[4][4],
    const float a[4][4],
    const float b[4][4])
{
    float c[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            c[i][j] = 0.0f;
            c[i][j] += a[0][j] * b[i][0];
            c[i][j] += a[1][j] * b[i][1];
            c[i][j] += a[2][j] * b[i][2];
            c[i][j] += a[3][j] * b[i][3];
        }
    }
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
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
    const float s = sinf(angle);
    const float c = cosf(angle);
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
    const float f = 1.0f / tanf(fov / 2.0f);
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

////////////////////////////////////////////////////////////////////////////////
// Camera Implementation ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void camera_init(camera_t* camera)
{
    assert(camera);
    camera->x = 0.0f;
    camera->y = 0.0f;
    camera->z = 0.0f;
    camera->pitch = rad(0.0f);
    camera->yaw = rad(0.0f);
    camera->width = 640.0f;
    camera->height = 480.0f;
    camera->fov = rad(90.0f);
    camera->near = 0.1f;
    camera->far = 5000.0f;
    camera->dirty = true;
}

void camera_update(camera_t* camera)
{
    assert(camera);
    if (!camera->dirty) {
        return;
    }
    float a[4][4];
    float b[4][4];
    const float aspect = camera->width / camera->height;
    translate(a, -camera->x, -camera->y, -camera->z);
    rotate(b, cosf(camera->yaw), 0.0f, sinf(camera->yaw), camera->pitch);
    multiply(a, b, a);
    rotate(b, 0.0f, 1.0f, 0.0f, -camera->yaw);
    multiply(a, b, a);
    perspective(b, aspect, camera->fov, camera->near, camera->far);
    multiply(camera->matrix, b, a);
    camera->dirty = false;
}

void camera_move(
    camera_t* camera,
    const float x,
    const float y,
    const float z)
{
    assert(camera);
    if (!x && !y && !z) {
        return;
    }
    const float c = cosf(camera->yaw);
    const float a = sinf(camera->pitch);
    const float s = sinf(camera->yaw);
    camera->x += s * z + c * x;
    camera->y += y + z * a;
    camera->z -= c * z - s * x;
    camera->dirty = true;
}

void camera_rotate(
    camera_t* camera,
    const float pitch,
    const float yaw)
{
    assert(camera);
    if (!pitch && !yaw) {
        return;
    }
    const float e = PI / 2.0f - EPSILON;
    camera->pitch += rad(pitch);
    camera->yaw += rad(yaw);
    camera->pitch = clamp(camera->pitch, -e, e);
    camera->dirty = true;
}

void camera_size(
    camera_t* camera,
    const int width,
    const int height)
{
    assert(camera);
    assert(width > 0.0f);
    assert(height > 0.0f);
    camera->width = width;
    camera->height = height;
    camera->dirty = true;
}

void camera_position(
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

void camera_vector(
    const camera_t* camera,
    float* x,
    float* y,
    float* z)
{
    assert(camera);
    assert(x);
    assert(y);
    assert(z);
    *x = cosf(camera->yaw - rad(90)) * cosf(camera->pitch);
    *y = sinf(camera->pitch);
    *z = sinf(camera->yaw - rad(90)) * cosf(camera->pitch);
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
    const float points[][3] = {
        { x,     y,     z     },
        { x + a, y,     z     },
        { x,     y + b, z     },
        { x,     y,     z + c },
        { x + a, y + b, z     },
        { x,     y + b, z + c },
        { x + a, y,     z + c },
        { x + a, y + b, z + c },
    };
    float d, e, f;
    camera_vector(camera, &d, &e, &f);
    for (int i = 0; i < 8; i++) {
        float s = points[i][0] - camera->x;
        float t = points[i][1] - camera->y;
        float p = points[i][2] - camera->z;
        const float length = sqrtf(s * s + t * t + p * p);
        if (length < max3(a, b, c)) {
            return true;
        }
        s /= length;
        t /= length;
        p /= length;
        const float dot = d * s + e * t + f * p;
        if (acos(dot) < camera->fov / 1.5f) {
            return true;
        }
    }
    return false;
}