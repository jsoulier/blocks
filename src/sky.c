#include <SDL3/SDL.h>

#include "save.h"
#include "sky.h"

static const float SUNRISE_LENGTH = 30.0f;
static const float DAY_LENGTH = 60.0f;
static const float SUNSET_LENGTH = 30.0f;
static const float NIGHT_LENGTH = 60.0f;
static const float SPEED = 0.1f;
static const float AMBIENT_SCALE = 0.5f;
static const float SHADOW_Y = 30.0f;
static const float SHADOW_ORTHO = 300.0f;
static const float SHADOW_FAR = 300.0f;
static const int SHADOW_UPDATE_FRAMES = 300;

static const float NIGHT_SKY_TOP[3] = {0.02f, 0.02f, 0.1f};
static const float NIGHT_SKY_HORIZON[3] = {0.05f, 0.05f, 0.2f};
static const float SUNRISE_SKY_TOP[3] = {0.5f, 0.2f, 0.1f};
static const float SUNRISE_SKY_HORIZON[3] = {1.0f, 0.5f, 0.2f};
static const float DAY_SKY_TOP[3] = {0.5f, 0.7f, 1.0f};
static const float DAY_SKY_HORIZON[3] = {0.8f, 0.9f, 1.0f};
static const float SUNSET_SKY_TOP[3] = {0.5f, 0.2f, 0.1f};
static const float SUNSET_SKY_HORIZON[3] = {1.0f, 0.5f, 0.2f};
static const float NIGHT_AMBIENT[3] = {0.05f, 0.05f, 0.1f};
static const float DAY_AMBIENT[3] = {1.0f, 1.0f, 1.0f};

static void LerpColor(float output[4], const float a[3], const float b[3], float t)
{
    output[0] = a[0] + (b[0] - a[0]) * t;
    output[1] = a[1] + (b[1] - a[1]) * t;
    output[2] = a[2] + (b[2] - a[2]) * t;
    output[3] = 0.0f;
}

static void UpdateRender(Sky* sky)
{
    float total_length = SUNRISE_LENGTH + DAY_LENGTH + SUNSET_LENGTH + NIGHT_LENGTH;
    float sunrise_end = SUNRISE_LENGTH / total_length;
    float day_end = sunrise_end + DAY_LENGTH / total_length;
    float sunset_end = day_end + SUNSET_LENGTH / total_length;
    float ambient[4];
    float t;
    if (sky->time_of_day < sunrise_end)
    {
        t = sky->time_of_day / sunrise_end;
        LerpColor(sky->render.top, NIGHT_SKY_TOP, SUNRISE_SKY_TOP, t);
        LerpColor(sky->render.horizon, NIGHT_SKY_HORIZON, SUNRISE_SKY_HORIZON, t);
        LerpColor(ambient, NIGHT_AMBIENT, DAY_AMBIENT, t);
    }
    else if (sky->time_of_day < day_end)
    {
        t = (sky->time_of_day - sunrise_end) / (day_end - sunrise_end);
        LerpColor(sky->render.top, SUNRISE_SKY_TOP, DAY_SKY_TOP, t);
        LerpColor(sky->render.horizon, SUNRISE_SKY_HORIZON, DAY_SKY_HORIZON, t);
        LerpColor(ambient, DAY_AMBIENT, DAY_AMBIENT, t);
    }
    else if (sky->time_of_day < sunset_end)
    {
        t = (sky->time_of_day - day_end) / (sunset_end - day_end);
        LerpColor(sky->render.top, DAY_SKY_TOP, SUNSET_SKY_TOP, t);
        LerpColor(sky->render.horizon, DAY_SKY_HORIZON, SUNSET_SKY_HORIZON, t);
        LerpColor(ambient, DAY_AMBIENT, NIGHT_AMBIENT, t);
    }
    else
    {
        t = (sky->time_of_day - sunset_end) / (1.0f - sunset_end);
        LerpColor(sky->render.top, SUNSET_SKY_TOP, NIGHT_SKY_TOP, t);
        LerpColor(sky->render.horizon, SUNSET_SKY_HORIZON, NIGHT_SKY_HORIZON, t);
        LerpColor(ambient, NIGHT_AMBIENT, NIGHT_AMBIENT, t);
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
    float total_length = SUNRISE_LENGTH + DAY_LENGTH + SUNSET_LENGTH + NIGHT_LENGTH;
    float sunrise_end = SUNRISE_LENGTH / total_length;
    float day_end = sunrise_end + DAY_LENGTH / total_length;
    sky->time_of_day = sunrise_end + (day_end - sunrise_end) / 2.0f;
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
    float total_length = SUNRISE_LENGTH + DAY_LENGTH + SUNSET_LENGTH + NIGHT_LENGTH;
    sky->time_of_day += dt * SPEED / total_length;
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
