#include <SDL3/SDL.h>

#include "hud.inc"
#include "input.h"

static const float BUTTONS[HUD_BUTTON_COUNT][2] =
{
    {HUD_BUTTON_PLACE_X, HUD_BUTTON_PLACE_Y},
    {HUD_BUTTON_BREAK_X, HUD_BUTTON_BREAK_Y},
    {HUD_BUTTON_JUMP_X, HUD_BUTTON_JUMP_Y},
    {HUD_BUTTON_SPRINT_X, HUD_BUTTON_SPRINT_Y},
    {HUD_BUTTON_BLOCK_X, HUD_BUTTON_BLOCK_Y},
    {HUD_BUTTON_CONTROLLER_X, HUD_BUTTON_CONTROLLER_Y},
};

static SDL_Window* window;
static InputDevice device;
static float movement[3];
static float rotation[2];
static bool jump;
static bool sprint;
static bool break_block;
static bool select_block;
static bool place_block;
static int change_block;
static bool toggle_controller;
static bool reset_sky;
static SDL_FingerID buttons[HUD_BUTTON_COUNT];
static SDL_FingerID move_finger;
static SDL_FingerID look_finger;
static float move_origin[2];

void Input_Init(SDL_Window* in_window)
{
    window = in_window;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    int count;
    SDL_free(SDL_GetTouchDevices(&count));
    if (count)
    {
        device = INPUT_DEVICE_TOUCH;
    }
    else
    {
        device = INPUT_DEVICE_KEYBOARD_MOUSE;
    }
}

void Input_GetSafeArea(float safe[4])
{
    int width;
    int height;
    int pixel_width;
    int pixel_height;
    SDL_Rect area;
    SDL_GetWindowSize(window, &width, &height);
    SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    SDL_GetWindowSafeArea(window, &area);
    float x = width ? (float) pixel_width / width : 1.0f;
    float y = height ? (float) pixel_height / height : 1.0f;
    safe[0] = area.x * x;
    safe[1] = pixel_height - (area.y + area.h) * y;
    safe[2] = area.w * x;
    safe[3] = area.h * y;
}

static float GetTouch(const SDL_Event* event, float touch[2], float viewport[2])
{
    int width;
    int height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    viewport[0] = width;
    viewport[1] = height;
    touch[0] = event->tfinger.x * viewport[0];
    touch[1] = viewport[1] - event->tfinger.y * viewport[1];
    return HUD_SCALE(viewport[0], viewport[1]);
}

static void SetDevice(InputDevice in_device)
{
    if (device == in_device)
    {
        return;
    }
    device = in_device;
    SDL_zeroa(movement);
    SDL_zeroa(rotation);
    jump = false;
    sprint = false;
    break_block = false;
    select_block = false;
    place_block = false;
    change_block = 0;
    toggle_controller = false;
    reset_sky = false;
    SDL_zeroa(buttons);
    move_finger = 0;
    look_finger = 0;
    SDL_zeroa(move_origin);
}

SDL_AppResult Input_Event(const SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (event->key.repeat)
        {
            break;
        }
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
        {
            SDL_SetWindowRelativeMouseMode(window, false);
            SDL_SetWindowFullscreen(window, false);
        }
        else if (event->key.scancode == SDL_SCANCODE_F11)
        {
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
        }
        else if (!SDL_GetWindowRelativeMouseMode(window))
        {
            break;
        }
        else if (event->key.scancode == SDL_SCANCODE_F5)
        {
            toggle_controller = true;
        }
        else if (event->key.scancode == SDL_SCANCODE_T)
        {
            reset_sky = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_SetWindowRelativeMouseMode(window, true);
        }
        else if (event->button.button == SDL_BUTTON_LEFT)
        {
            break_block = true;
        }
        else if (event->button.button == SDL_BUTTON_MIDDLE)
        {
            select_block = true;
        }
        else if (event->button.button == SDL_BUTTON_RIGHT)
        {
            place_block = true;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            change_block += event->wheel.y;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            rotation[0] += event->motion.xrel;
            rotation[1] += event->motion.yrel;
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
    {
        SetDevice(INPUT_DEVICE_TOUCH);
        float touch[2];
        float viewport[2];
        float scale = GetTouch(event, touch, viewport);
        float safe[4];
        Input_GetSafeArea(safe);
        int index = -1;
        float radius = HUD_BUTTON_RADIUS * scale;
        for (int i = 0; i < HUD_BUTTON_COUNT; i++)
        {
            float x = HUD_BUTTON_CENTER_X(safe[0], safe[2], i, BUTTONS[i][0], scale) - touch[0];
            float y = HUD_BUTTON_CENTER_Y(safe[1], BUTTONS[i][1], scale) - touch[1];
            if (x * x + y * y <= radius * radius)
            {
                index = i;
            }
        }
        if (index >= 0)
        {
            buttons[index] = event->tfinger.fingerID;
            place_block |= index == HUD_BUTTON_PLACE;
            break_block |= index == HUD_BUTTON_BREAK;
            change_block += index == HUD_BUTTON_BLOCK;
            toggle_controller |= index == HUD_BUTTON_CONTROLLER;
        }
        else if (touch[0] < viewport[0] * 0.5f)
        {
            move_finger = event->tfinger.fingerID;
            SDL_memcpy(move_origin, touch, sizeof(move_origin));
        }
        else
        {
            look_finger = event->tfinger.fingerID;
        }
        break;
    }
    case SDL_EVENT_FINGER_MOTION:
    {
        SetDevice(INPUT_DEVICE_TOUCH);
        float touch[2];
        float viewport[2];
        float scale = GetTouch(event, touch, viewport);
        if (event->tfinger.fingerID == move_finger)
        {
            float radius = HUD_STICK_RADIUS * scale;
            movement[0] = SDL_clamp((touch[0] - move_origin[0]) / radius, -1.0f, 1.0f);
            movement[2] = SDL_clamp((touch[1] - move_origin[1]) / radius, -1.0f, 1.0f);
        }
        else if (event->tfinger.fingerID == look_finger)
        {
            rotation[0] += event->tfinger.dx * viewport[0];
            rotation[1] += event->tfinger.dy * viewport[1];
        }
        break;
    }
    case SDL_EVENT_FINGER_UP:
        if (event->tfinger.fingerID == move_finger)
        {
            move_finger = 0;
            SDL_zeroa(movement);
        }
        else if (event->tfinger.fingerID == look_finger)
        {
            look_finger = 0;
        }
        else
        {
            for (int i = 0; i < HUD_BUTTON_COUNT; i++)
            {
                if (buttons[i] == event->tfinger.fingerID)
                {
                    buttons[i] = 0;
                }
            }
        }
        break;
    }
    return SDL_APP_CONTINUE;
}

void Input_Update()
{
    SDL_zeroa(rotation);
    change_block = 0;
    break_block = false;
    place_block = false;
    select_block = false;
    toggle_controller = false;
    reset_sky = false;
    switch (device)
    {
    case INPUT_DEVICE_KEYBOARD_MOUSE:
    {
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_zeroa(movement);
            jump = false;
            sprint = false;
            break;
        }
        const bool* keys = SDL_GetKeyboardState(NULL);
        bool up = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_E];
        bool down = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_Q];
        movement[0] = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        movement[1] = up - down;
        movement[2] = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
        jump = keys[SDL_SCANCODE_SPACE];
        sprint = keys[SDL_SCANCODE_LCTRL];
        break;
    }
    case INPUT_DEVICE_TOUCH:
        jump = buttons[HUD_BUTTON_JUMP] != 0;
        sprint = buttons[HUD_BUTTON_SPRINT] != 0;
        break;
    case INPUT_DEVICE_GAMEPAD:
        // TODO:
        break;
    case INPUT_DEVICE_COUNT:
        break;
    }
}

InputDevice Input_GetDevice()
{
    return device;
}

void Input_GetMovement(float in_movement[3])
{
    SDL_memcpy(in_movement, movement, sizeof(movement));
}

void Input_GetRotation(float in_rotation[2])
{
    SDL_memcpy(in_rotation, rotation, sizeof(rotation));
}

bool Input_GetJump()
{
    return jump;
}

bool Input_GetSprint()
{
    return sprint;
}

bool Input_GetBreakBlock()
{
    return break_block;
}

bool Input_GetSelectBlock()
{
    return select_block;
}

bool Input_GetPlaceBlock()
{
    return place_block;
}

int Input_GetChangeBlock()
{
    return change_block;
}

bool Input_GetToggleController()
{
    return toggle_controller;
}

bool Input_GetResetSky()
{
    return reset_sky;
}
