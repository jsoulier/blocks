#include <SDL3/SDL.h>

#include "hud.inc"
#include "input.h"

static const float BUTTONS[HUD_BUTTON_COUNT][2] =
{
    {HUD_BUTTON_PLACE_X, HUD_BUTTON_PLACE_Y},
    {HUD_BUTTON_BREAK_X, HUD_BUTTON_BREAK_Y},
    {HUD_BUTTON_JUMP_X, HUD_BUTTON_JUMP_Y},
    {HUD_BUTTON_SPRINT_X, HUD_BUTTON_SPRINT_Y},
    {HUD_BUTTON_BLOCK_NEXT_X, HUD_BUTTON_BLOCK_NEXT_Y},
    {HUD_BUTTON_CONTROLLER_X, HUD_BUTTON_CONTROLLER_Y},
    {HUD_BUTTON_BLOCK_PREV_X, HUD_BUTTON_BLOCK_PREV_Y},
};

static const float GAMEPAD_DEADZONE = 0.2f;
static const float GAMEPAD_THRESHOLD = 0.5f;
static const float GAMEPAD_SENSITIVITY = 2.0f;

static SDL_Window* window;                     // k&m / touch / gamepad
static InputDevice device;                     // k&m / touch / gamepad
static float movement[3];                      // k&m / touch / gamepad
static float rotation[2];                      // k&m / touch / gamepad
static bool jump;                              // k&m / touch / gamepad
static bool sprint;                            // k&m / touch / gamepad
static bool break_block;                       // k&m / touch / gamepad
static bool select_block;                      // k&m / touch / gamepad
static bool place_block;                       // k&m / touch / gamepad
static int change_block;                       // k&m / touch / gamepad
static bool toggle_controller;                 // k&m / touch / gamepad
static bool reset_sky;                         // k&m / touch / gamepad
static SDL_FingerID buttons[HUD_BUTTON_COUNT]; // touch
static SDL_FingerID move_finger;               // touch
static SDL_FingerID look_finger;               // touch
static float move_origin[2];                   // touch
static SDL_Gamepad* gamepad;                   // gamepad
static bool triggers[2];                       // gamepad

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

void Input_Free()
{
    SDL_CloseGamepad(gamepad);
    gamepad = NULL;
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
    float scale = HUD_SCALE(viewport[0], viewport[1]);
    return scale;
}

static float GetStick(SDL_GamepadAxis axis)
{
    float value = SDL_GetGamepadAxis(gamepad, axis) / (float) SDL_JOYSTICK_AXIS_MAX;
    if (SDL_fabsf(value) > GAMEPAD_DEADZONE)
    {
        return (value - SDL_copysignf(GAMEPAD_DEADZONE, value)) / (1.0f - GAMEPAD_DEADZONE);
    }
    else
    {
        return 0.0f;
    }
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
    SDL_zeroa(triggers);
}

SDL_AppResult Input_Event(const SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
    {
        return SDL_APP_SUCCESS;
    }
    case SDL_EVENT_KEY_DOWN:
    {
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
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    {
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
    }
    case SDL_EVENT_MOUSE_WHEEL:
    {
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            change_block += event->wheel.y;
        }
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
    {
        SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            rotation[0] += event->motion.xrel;
            rotation[1] += event->motion.yrel;
        }
        break;
    }
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
            change_block += index == HUD_BUTTON_BLOCK_NEXT;
            change_block -= index == HUD_BUTTON_BLOCK_PREV;
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
    {
        SetDevice(INPUT_DEVICE_TOUCH);
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
    case SDL_EVENT_GAMEPAD_ADDED:
    {
        if (!gamepad)
        {
            gamepad = SDL_OpenGamepad(event->gdevice.which);
            if (!gamepad)
            {
                SDL_Log("Failed to open gamepad: %s", SDL_GetError());
            }
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
    {
        if (gamepad && event->gdevice.which == SDL_GetGamepadID(gamepad))
        {
            SDL_CloseGamepad(gamepad);
            gamepad = NULL;
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    {
        SetDevice(INPUT_DEVICE_GAMEPAD);
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_WEST)
        {
            reset_sky = true;
        }
        else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_NORTH)
        {
            toggle_controller = true;
        }
        else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK)
        {
            select_block = true;
        }
        else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
        {
            change_block--;
        }
        else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
        {
            change_block++;
        }
        else if (event->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK)
        {
            sprint = true;
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        if (SDL_abs(event->gaxis.value) > GAMEPAD_DEADZONE * SDL_JOYSTICK_AXIS_MAX)
        {
            SetDevice(INPUT_DEVICE_GAMEPAD);
        }
        if (event->gaxis.axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && event->gaxis.axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
        {
            break;
        }
        int index = event->gaxis.axis - SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        bool held = event->gaxis.value > GAMEPAD_THRESHOLD * SDL_JOYSTICK_AXIS_MAX;
        if (held && !triggers[index])
        {
            place_block |= index == 0;
            break_block |= index == 1;
        }
        triggers[index] = held;
        break;
    }
    }
    return SDL_APP_CONTINUE;
}

void Input_Update(float dt)
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
    {
        jump = buttons[HUD_BUTTON_JUMP] != 0;
        sprint = buttons[HUD_BUTTON_SPRINT] != 0;
        break;
    }
    case INPUT_DEVICE_GAMEPAD:
    {
        if (!gamepad)
        {
            SetDevice(INPUT_DEVICE_KEYBOARD_MOUSE);
            break;
        }
        bool up = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
        bool down = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
        movement[0] = GetStick(SDL_GAMEPAD_AXIS_LEFTX);
        movement[1] = up - down;
        movement[2] = -GetStick(SDL_GAMEPAD_AXIS_LEFTY);
        rotation[0] += GetStick(SDL_GAMEPAD_AXIS_RIGHTX) * GAMEPAD_SENSITIVITY * dt;
        rotation[1] += GetStick(SDL_GAMEPAD_AXIS_RIGHTY) * GAMEPAD_SENSITIVITY * dt;
        jump = up;
        if (!movement[0] && !movement[2])
        {
            sprint = false;
        }
        break;
    }
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
