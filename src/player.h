// move to main

#pragma once

#include "camera.h"
#include "world.h"

typedef struct player
{
    int id;
    camera_t camera;
    world_query_t query;
    block_t block;
}
player_t;

void player_init(player_t* player);
void player_move(player_t* player, float dt);
void player_rotate(player_t* player, float pitch, float yaw);
void player_break(player_t* player);
void player_place(player_t* player);
void player_scroll(player_t* player, int dy);
