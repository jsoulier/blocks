#include <SDL3/SDL.h>

#include "camera.h"
#include "player.h"
#include "world.h"

static const float SPEED = 0.01f;
static const float SENSITIVITY = 0.1f;
static const float REACH = 10.0f;

static void query(player_t* player)
{
    float x;
    float y;
    float z;
    float dx;
    float dy;
    float dz;
    camera_get_position(&player->camera, &x, &y, &z);
    camera_get_vector(&player->camera, &dx, &dy, &dz);
    player->query = world_query(x, y, z, dx, dy, dz, REACH);
}

void player_init(player_t* player)
{
    player->id = 0;
    camera_init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player->block = BLOCK_YELLOW_TORCH;
}

void player_move(player_t* player, float dt)
{
    float speed = SPEED;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    const bool* state = SDL_GetKeyboardState(NULL);
    dx += state[SDL_SCANCODE_D];
    dx -= state[SDL_SCANCODE_A];
    dy += state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE];
    dy -= state[SDL_SCANCODE_Q] || state[SDL_SCANCODE_LSHIFT];
    dz += state[SDL_SCANCODE_W];
    dz -= state[SDL_SCANCODE_S];
    if (state[SDL_SCANCODE_LCTRL])
    {
        speed *= 5.0f;
    }
    dx *= speed * dt;
    dy *= speed * dt;
    dz *= speed * dt;
    camera_move(&player->camera, dx, dy, dz);
    query(player);
}

void player_rotate(player_t* player, float pitch, float yaw)
{
    pitch *= -SENSITIVITY;
    yaw *= SENSITIVITY;
    camera_rotate(&player->camera, pitch, yaw);
    query(player);
}

void player_break(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        world_set_block(player->query.current, BLOCK_EMPTY);
    }
}

void player_place(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        world_set_block(player->query.previous, player->block);
    }
}

void player_scroll(player_t* player, int dy)
{
    int value = player->block;
    value = (value + dy) % BLOCK_COUNT;
    value = SDL_max(value, 1);
    player->block = value;
}
