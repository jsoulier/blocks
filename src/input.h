#pragma once

#include <SDL3/SDL.h>

typedef enum InputDevice
{
    INPUT_DEVICE_KEYBOARD_MOUSE,
    INPUT_DEVICE_GAMEPAD,
    INPUT_DEVICE_TOUCH,
    INPUT_DEVICE_COUNT,
} InputDevice;

void Input_Init(SDL_Window* window);
void Input_Free();
void Input_GetSafeArea(float safe[4]);
SDL_AppResult Input_Event(const SDL_Event* event);
void Input_Update(float dt);
InputDevice Input_GetDevice();
void Input_GetMovement(float movement[3]);
void Input_GetRotation(float rotation[2]);
bool Input_GetJump();
bool Input_GetSprint();
bool Input_GetBreakBlock();
bool Input_GetSelectBlock();
bool Input_GetPlaceBlock();
int Input_GetChangeBlock();
bool Input_GetToggleController();
bool Input_GetResetSky();
