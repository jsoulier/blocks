#include <SDL3/SDL.h>

#include "save.h"
#include "sky.h"

static const float SUNRISE_LENGTH = 30.0f;
static const float DAY_LENGTH = 60.0f;
static const float SUNSET_LENGTH = 30.0f;
static const float NIGHT_LENGTH = 60.0f;
static const float SPEED = 1.0f;
static const float AMBIENT_SCALE = 0.5f;

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

static void lerp_color(float output[4], const float a[3], const float b[3], float t)
{
    output[0] = a[0] + (b[0] - a[0]) * t;
    output[1] = a[1] + (b[1] - a[1]) * t;
    output[2] = a[2] + (b[2] - a[2]) * t;
    output[3] = 0.0f;
}

static void update_render(sky_t* sky)
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
        lerp_color(sky->render.top, NIGHT_SKY_TOP, SUNRISE_SKY_TOP, t);
        lerp_color(sky->render.horizon, NIGHT_SKY_HORIZON, SUNRISE_SKY_HORIZON, t);
        lerp_color(ambient, NIGHT_AMBIENT, DAY_AMBIENT, t);
    }
    else if (sky->time_of_day < day_end)
    {
        t = (sky->time_of_day - sunrise_end) / (day_end - sunrise_end);
        lerp_color(sky->render.top, SUNRISE_SKY_TOP, DAY_SKY_TOP, t);
        lerp_color(sky->render.horizon, SUNRISE_SKY_HORIZON, DAY_SKY_HORIZON, t);
        lerp_color(ambient, DAY_AMBIENT, DAY_AMBIENT, t);
    }
    else if (sky->time_of_day < sunset_end)
    {
        t = (sky->time_of_day - day_end) / (sunset_end - day_end);
        lerp_color(sky->render.top, DAY_SKY_TOP, SUNSET_SKY_TOP, t);
        lerp_color(sky->render.horizon, DAY_SKY_HORIZON, SUNSET_SKY_HORIZON, t);
        lerp_color(ambient, DAY_AMBIENT, NIGHT_AMBIENT, t);
    }
    else
    {
        t = (sky->time_of_day - sunset_end) / (1.0f - sunset_end);
        lerp_color(sky->render.top, SUNSET_SKY_TOP, NIGHT_SKY_TOP, t);
        lerp_color(sky->render.horizon, SUNSET_SKY_HORIZON, NIGHT_SKY_HORIZON, t);
        lerp_color(ambient, NIGHT_AMBIENT, NIGHT_AMBIENT, t);
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

void sky_save_or_load(sky_t* sky, bool save)
{
    SDL_COMPILE_TIME_ASSERT("", sizeof(sky_render_t) == sizeof(float) * 16);
    if (save)
    {
        save_set_sky(sky->time_of_day);
        return;
    }
    float total_length = SUNRISE_LENGTH + DAY_LENGTH + SUNSET_LENGTH + NIGHT_LENGTH;
    float sunrise_end = SUNRISE_LENGTH / total_length;
    float day_end = sunrise_end + DAY_LENGTH / total_length;
    sky->time_of_day = sunrise_end + (day_end - sunrise_end) / 2.0f;
    float saved_time;
    if (save_get_sky(&saved_time) && saved_time >= 0.0f && saved_time < 1.0f)
    {
        sky->time_of_day = saved_time;
    }
    update_render(sky);
}

void sky_update(sky_t* sky, float dt)
{
    float total_length = SUNRISE_LENGTH + DAY_LENGTH + SUNSET_LENGTH + NIGHT_LENGTH;
    sky->time_of_day += dt * SPEED / total_length;
    sky->time_of_day = SDL_fmodf(sky->time_of_day, 1.0f);
    update_render(sky);
}
