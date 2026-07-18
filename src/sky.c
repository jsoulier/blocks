#include <SDL3/SDL.h>
#include <stddef.h>

#include "save.h"
#include "sky.h"

static const float TOTAL_LENGTH = 180.0f;
static const float SUNRISE_END = 30.0f / 180.0f;
static const float DAY_END = 90.0f / 180.0f;
static const float SUNSET_END = 120.0f / 180.0f;
static const float SPEED = 0.1f;
static const float AMBIENT_SCALE = 0.35f;
static const float SHADOW_Y = 30.0f;
static const float SHADOW_ORTHO = 300.0f;
static const float SHADOW_FAR = 300.0f;
static const int SHADOW_UPDATE_FRAMES = 300;
static const float NIGHT_SKY_TOP[3] = {0.01f, 0.02f, 0.06f};
static const float NIGHT_SKY_HORIZON[3] = {0.06f, 0.08f, 0.18f};
static const float TWILIGHT_SKY_TOP[3] = {0.24f, 0.16f, 0.32f};
static const float TWILIGHT_SKY_HORIZON[3] = {0.95f, 0.45f, 0.28f};
static const float DAY_SKY_TOP[3] = {0.32f, 0.58f, 0.92f};
static const float DAY_SKY_HORIZON[3] = {0.72f, 0.84f, 0.98f};
static const float NIGHT_AMBIENT[3] = {0.12f, 0.16f, 0.28f};
static const float DAY_AMBIENT[3] = {0.85f, 0.9f, 1.0f};

static void Lerp(float out[4], const float start[3], const float end[3], float alpha)
{
    out[0] = start[0] + (end[0] - start[0]) * alpha;
    out[1] = start[1] + (end[1] - start[1]) * alpha;
    out[2] = start[2] + (end[2] - start[2]) * alpha;
    out[3] = 0.0f;
}

static void Copy(float out[4], const float in[3])
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
    out[3] = 0.0f;
}

void Sky_Reset(Sky* sky)
{
    sky->time_of_day = 3.0f / 8.0f;
}

void Sky_Save(const Sky* sky)
{
    Save_SetSky(sky->time_of_day);
}

void Sky_Load(Sky* sky)
{
    SDL_COMPILE_TIME_ASSERT("", offsetof(Sky, time_of_day) == sizeof(float) * 16);
    Camera_Init(&sky->camera, CAMERA_TYPE_ORTHO);
    sky->camera.ortho = SHADOW_ORTHO;
    sky->camera.far = SHADOW_FAR;
    sky->frame = 0;
    sky->time_of_day = SUNRISE_END + (DAY_END - SUNRISE_END) / 2.0f;
    float time;
    if (Save_GetSky(&time) && time >= 0.0f && time < 1.0f)
    {
        sky->time_of_day = time;
    }
}

void Sky_Update(Sky* sky, const Camera* camera, int resolution, float dt)
{
    sky->time_of_day += dt * SPEED / TOTAL_LENGTH;
    sky->time_of_day = SDL_fmodf(sky->time_of_day, 1.0f);
    float alpha;
    if (sky->time_of_day < SUNRISE_END)
    {
        alpha = sky->time_of_day / SUNRISE_END;
        Lerp(sky->top, NIGHT_SKY_TOP, TWILIGHT_SKY_TOP, alpha);
        Lerp(sky->horizon, NIGHT_SKY_HORIZON, TWILIGHT_SKY_HORIZON, alpha);
        Lerp(sky->ambient, NIGHT_AMBIENT, DAY_AMBIENT, alpha);
    }
    else if (sky->time_of_day < DAY_END)
    {
        alpha = (sky->time_of_day - SUNRISE_END) / (DAY_END - SUNRISE_END);
        Lerp(sky->top, TWILIGHT_SKY_TOP, DAY_SKY_TOP, alpha);
        Lerp(sky->horizon, TWILIGHT_SKY_HORIZON, DAY_SKY_HORIZON, alpha);
        Copy(sky->ambient, DAY_AMBIENT);
    }
    else if (sky->time_of_day < SUNSET_END)
    {
        alpha = (sky->time_of_day - DAY_END) / (SUNSET_END - DAY_END);
        Lerp(sky->top, DAY_SKY_TOP, TWILIGHT_SKY_TOP, alpha);
        Lerp(sky->horizon, DAY_SKY_HORIZON, TWILIGHT_SKY_HORIZON, alpha);
        Lerp(sky->ambient, DAY_AMBIENT, NIGHT_AMBIENT, alpha);
    }
    else
    {
        alpha = (sky->time_of_day - SUNSET_END) / (1.0f - SUNSET_END);
        Lerp(sky->top, TWILIGHT_SKY_TOP, NIGHT_SKY_TOP, alpha);
        Lerp(sky->horizon, TWILIGHT_SKY_HORIZON, NIGHT_SKY_HORIZON, alpha);
        Copy(sky->ambient, NIGHT_AMBIENT);
    }
    float angle = sky->time_of_day * 2.0f * SDL_PI_F - SDL_PI_F * 0.5f;
    float s = SDL_sinf(angle);
    float c = SDL_cosf(angle);
    float intensity = SDL_clamp(s, 0.0f, 1.0f);
    sky->sun[0] = c * SDL_sqrtf(0.5f);
    sky->sun[1] = -intensity;
    sky->sun[2] = -c * SDL_sqrtf(0.5f);
    sky->sun[3] = intensity;
    sky->is_shadow_on = intensity > 0.0f;
    float ambient = SDL_clamp(intensity, 0.1f, 1.0f) * AMBIENT_SCALE;
    sky->ambient[0] *= ambient;
    sky->ambient[1] *= ambient;
    sky->ambient[2] *= ambient;
    Camera* shadow = &sky->camera;
    if (sky->frame == 0)
    {
        shadow->pitch = SDL_asinf(sky->sun[1]);
        shadow->yaw = SDL_atan2f(sky->sun[0], -sky->sun[2]);
    }
    sky->frame = (sky->frame + 1) % SHADOW_UPDATE_FRAMES;
    shadow->x = camera->x;
    shadow->y = SHADOW_Y;
    shadow->z = camera->z;
    Camera_Snap(shadow, resolution);
}
