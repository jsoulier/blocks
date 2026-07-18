#include <SDL3/SDL.h>

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

static void LerpColor(float output[4], const float start[3], const float end[3], float amount)
{
    output[0] = start[0] + (end[0] - start[0]) * amount;
    output[1] = start[1] + (end[1] - start[1]) * amount;
    output[2] = start[2] + (end[2] - start[2]) * amount;
    output[3] = 0.0f;
}

static void CopyColor(float output[4], const float color[3])
{
    output[0] = color[0];
    output[1] = color[1];
    output[2] = color[2];
    output[3] = 0.0f;
}

static void UpdateRender(Sky* sky)
{
    float ambient[4];
    float progress;
    if (sky->time_of_day < SUNRISE_END)
    {
        progress = sky->time_of_day / SUNRISE_END;
        LerpColor(sky->render.top, NIGHT_SKY_TOP, TWILIGHT_SKY_TOP, progress);
        LerpColor(sky->render.horizon, NIGHT_SKY_HORIZON, TWILIGHT_SKY_HORIZON, progress);
        LerpColor(ambient, NIGHT_AMBIENT, DAY_AMBIENT, progress);
    }
    else if (sky->time_of_day < DAY_END)
    {
        progress = (sky->time_of_day - SUNRISE_END) / (DAY_END - SUNRISE_END);
        LerpColor(sky->render.top, TWILIGHT_SKY_TOP, DAY_SKY_TOP, progress);
        LerpColor(sky->render.horizon, TWILIGHT_SKY_HORIZON, DAY_SKY_HORIZON, progress);
        CopyColor(ambient, DAY_AMBIENT);
    }
    else if (sky->time_of_day < SUNSET_END)
    {
        progress = (sky->time_of_day - DAY_END) / (SUNSET_END - DAY_END);
        LerpColor(sky->render.top, DAY_SKY_TOP, TWILIGHT_SKY_TOP, progress);
        LerpColor(sky->render.horizon, DAY_SKY_HORIZON, TWILIGHT_SKY_HORIZON, progress);
        LerpColor(ambient, DAY_AMBIENT, NIGHT_AMBIENT, progress);
    }
    else
    {
        progress = (sky->time_of_day - SUNSET_END) / (1.0f - SUNSET_END);
        LerpColor(sky->render.top, TWILIGHT_SKY_TOP, NIGHT_SKY_TOP, progress);
        LerpColor(sky->render.horizon, TWILIGHT_SKY_HORIZON, NIGHT_SKY_HORIZON, progress);
        CopyColor(ambient, NIGHT_AMBIENT);
    }
    float angle = sky->time_of_day * 2.0f * SDL_PI_F;
    float sun_height = -SDL_cosf(angle);
    float sun_intensity = SDL_clamp(sun_height, 0.0f, 1.0f);
    float horizontal = SDL_sinf(angle);
    sky->render.sun[0] = horizontal * SDL_sqrtf(0.5f);
    sky->render.sun[1] = -sun_intensity;
    sky->render.sun[2] = -horizontal * SDL_sqrtf(0.5f);
    sky->render.sun[3] = sun_intensity;
    sky->has_sun = sun_intensity > 0.0f;
    float ambient_energy = SDL_clamp(sun_intensity, 0.1f, 1.0f) * AMBIENT_SCALE;
    sky->render.ambient[0] = ambient[0] * ambient_energy;
    sky->render.ambient[1] = ambient[1] * ambient_energy;
    sky->render.ambient[2] = ambient[2] * ambient_energy;
    sky->render.ambient[3] = 0.0f;
}

void Sky_Load(Sky* sky)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(SkyRender) == sizeof(float) * 16);
    Camera_Init(&sky->shadow_camera, CAMERA_TYPE_ORTHO);
    sky->shadow_camera.ortho = SHADOW_ORTHO;
    sky->shadow_camera.far = SHADOW_FAR;
    sky->shadow_frame = 0;
    sky->time_of_day = SUNRISE_END + (DAY_END - SUNRISE_END) / 2.0f;
    float saved_time;
    if (Save_GetSky(&saved_time) && saved_time >= 0.0f && saved_time < 1.0f)
    {
        sky->time_of_day = saved_time;
    }
    UpdateRender(sky);
}

void Sky_Reset(Sky* sky)
{
    sky->time_of_day = 3.0f / 8.0f;
    sky->shadow_frame = 0;
    UpdateRender(sky);
}

void Sky_Update(Sky* sky, float dt)
{
    sky->time_of_day += dt * SPEED / TOTAL_LENGTH;
    sky->time_of_day = SDL_fmodf(sky->time_of_day, 1.0f);
    UpdateRender(sky);
}

void Sky_Save(const Sky* sky)
{
    Save_SetSky(sky->time_of_day);
}

void Sky_UpdateShadow(Sky* sky, const Camera* camera, int resolution)
{
    Camera* shadow = &sky->shadow_camera;
    if (sky->shadow_frame == 0)
    {
        shadow->pitch = SDL_asinf(sky->render.sun[1]);
        shadow->yaw = SDL_atan2f(sky->render.sun[0], -sky->render.sun[2]);
    }
    sky->shadow_frame = (sky->shadow_frame + 1) % SHADOW_UPDATE_FRAMES;
    shadow->x = camera->x;
    shadow->y = SHADOW_Y;
    shadow->z = camera->z;
    Camera_Update(shadow);
    float texel_size = SHADOW_ORTHO * 2.0f / resolution;
    float light_x = shadow->view[0][0] * shadow->x + shadow->view[1][0] * shadow->y + shadow->view[2][0] * shadow->z;
    float light_y = shadow->view[0][1] * shadow->x + shadow->view[1][1] * shadow->y + shadow->view[2][1] * shadow->z;
    float delta_x = SDL_roundf(light_x / texel_size) * texel_size - light_x;
    float delta_y = SDL_roundf(light_y / texel_size) * texel_size - light_y;
    shadow->x += shadow->view[0][0] * delta_x + shadow->view[0][1] * delta_y;
    shadow->y += shadow->view[1][0] * delta_x + shadow->view[1][1] * delta_y;
    shadow->z += shadow->view[2][0] * delta_x + shadow->view[2][1] * delta_y;
    Camera_Update(shadow);
}
