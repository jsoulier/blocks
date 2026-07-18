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
void Player_ToggleController(Player* player);
void Player_Rotate(Player* player, float pitch, float yaw);
void Player_Move(Player* player, float dt);
void Player_PlaceBlock(const Player* player);
void Player_SelectBlock(Player* player);
void Player_BreakBlock(const Player* player);
void Player_ChangeBlock(Player* player, int dy);
