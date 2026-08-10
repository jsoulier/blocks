#pragma once

#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
#include "world.h"

typedef enum PlayerController
{
    PLAYER_CONTROLLER_WALK,
    PLAYER_CONTROLLER_FLY,
    PLAYER_CONTROLLER_COUNT,
} PlayerController;

typedef struct Player
{
    Camera camera;
    PlayerController controller;
    float velocity[3];
    bool is_on_ground;
    WorldQuery query;
    Block block;
} Player;

void Player_Load(Player* player);
void Player_Save(const Player* player);
void Player_Update(Player* player, float dt);
