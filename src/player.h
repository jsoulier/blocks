#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"

typedef enum player_controller
{
    PLAYER_CONTROLLER_FP,
    PLAYER_CONTROLLER_FREECAM,
}
player_controller_t;

typedef struct player
{
    camera_t camera;
    float velocity[3];
    bool on_ground;
    bool jump_was_down;
    block_t block;
    player_controller_t controller;
}
player_t;

void player_init(player_t* player);
void player_set_controller(player_t* player, player_controller_t controller);
void player_toggle_controller(player_t* player);
const char* player_controller_name(player_controller_t controller);
void player_rotate(player_t* player, float pitch, float yaw, float sensitivity);
void player_move(player_t* player, float dt_ms, const bool* keyboard_state);
bool player_overlaps_block(const player_t* player, const int position[3]);
void player_update_grounded(player_t* player);
