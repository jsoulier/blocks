#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
#include "input.h"
#include "player.h"
#include "save.h"
#include "world.h"

typedef struct PlayerSave
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    Block block;
} PlayerSave;

static const float EPSILON = 0.001f;
static const float WALK_SPEED = 4.0f;
static const float SPRINT_MULTIPLER = 2.5f;
static const float SENSITIVITY = 0.1f;
static const float REACH = 10.0f;
static const float AIR_ACCELERATION = 6.0f;
static const float GRAVITY = 24.0f;
static const float JUMP_SPEED = 8.5f;
static const float FLY_SPEED = 0.05f;
static const float COLLISION_STEP = 0.1f;
static const float GROUND_OFFSET = 0.002f;
static const float AABB[2][3] = {{-0.3f, -1.62f, -0.3f}, {0.3f, 1.8f - 1.62f, 0.3f}};

static bool IsColliding(const float position[3])
{
    int min[3];
    int max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = SDL_floorf(position[i] + AABB[0][i] + EPSILON);
        max[i] = SDL_floorf(position[i] + AABB[1][i] - EPSILON);
    }
    for (int bx = min[0]; bx <= max[0]; bx++)
    for (int by = min[1]; by <= max[1]; by++)
    for (int bz = min[2]; bz <= max[2]; bz++)
    {
        int block[3] = {bx, by, bz};
        if (Block_IsSolid(World_GetBlock(block)))
        {
            return true;
        }
    }
    return false;
}

static void Bisect(float position[3], int axis, float step)
{
    float start[3] = {position[0], position[1], position[2]};
    float lower = 0.0f;
    float upper = 1.0f;
    for (int i = 0; i < 8; i++)
    {
        float half = (lower + upper) * 0.5f;
        float location[3] = {start[0], start[1], start[2]};
        location[axis] += step * half;
        if (IsColliding(location))
        {
            upper = half;
        }
        else
        {
            lower = half;
        }
    }
    position[axis] = start[axis] + step * lower;
}

static bool MoveAxis(float position[3], int axis, float delta)
{
    if (SDL_fabsf(delta) <= SDL_FLT_EPSILON)
    {
        return false;
    }
    int steps = SDL_max(SDL_ceilf(SDL_fabsf(delta) / COLLISION_STEP), 1);
    float step = delta / steps;
    for (int i = 0; i < steps; i++)
    {
        float location[3] = {position[0], position[1], position[2]};
        location[axis] += step;
        if (IsColliding(location))
        {
            Bisect(position, axis, step);
            return true;
        }
        SDL_memcpy(position, location, sizeof(location));
    }
    return false;
}

void Player_Load(Player* player)
{
    PlayerSave save;
    Camera_Init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player->camera.x = -200.0f;
    player->camera.y = 50.0f;
    player->camera.z = 0.0f;
    player->controller = PLAYER_CONTROLLER_WALK;
    player->block = BLOCK_YELLOW_TORCH;
    if (Save_GetPlayer(&save, sizeof(save)))
    {
        player->block = save.block;
        player->camera.x = save.x;
        player->camera.y = save.y;
        player->camera.z = save.z;
        player->camera.pitch = save.pitch;
        player->camera.yaw = save.yaw;
    }
    SDL_memset(&player->query, 0, sizeof(player->query));
}

void Player_Save(const Player* player)
{
    PlayerSave save;
    save.x = player->camera.x;
    save.y = player->camera.y;
    save.z = player->camera.z;
    save.pitch = player->camera.pitch;
    save.yaw = player->camera.yaw;
    save.block = player->block;
    Save_SetPlayer(&save, sizeof(save));
}

static void Move(Player* player, float dt)
{
    float delta[3];
    Input_GetMovement(delta);
    bool sprint = Input_GetSprint();
    if (player->controller == PLAYER_CONTROLLER_FLY)
    {
        float speed = FLY_SPEED * (sprint ? SPRINT_MULTIPLER : 1.0f);
        Camera_Move(&player->camera, delta[0] * speed * dt, delta[1] * speed * dt, delta[2] * speed * dt);
        return;
    }
    dt = SDL_min(dt * 0.001f, 0.05f);
    float right = delta[0];
    float forward = delta[2];
    float length = SDL_sqrtf(right * right + forward * forward);
    if (length > SDL_FLT_EPSILON)
    {
        right /= length;
        forward /= length;
    }
    float speed = WALK_SPEED * (sprint ? SPRINT_MULTIPLER : 1.0f);
    float sy = SDL_sinf(player->camera.yaw);
    float cy = SDL_cosf(player->camera.yaw);
    float dx = (cy * right + sy * forward) * speed;
    float dz = (sy * right - cy * forward) * speed;
    if (player->is_on_ground)
    {
        player->velocity[0] = dx;
        player->velocity[2] = dz;
    }
    else
    {
        player->velocity[0] += (dx - player->velocity[0]) * AIR_ACCELERATION * dt;
        player->velocity[2] += (dz - player->velocity[2]) * AIR_ACCELERATION * dt;
    }
    if (Input_GetJump() && player->is_on_ground)
    {
        player->velocity[1] = JUMP_SPEED;
        player->is_on_ground = false;
    }
    if (dt <= SDL_FLT_EPSILON)
    {
        return;
    }
    player->velocity[1] -= GRAVITY * dt;
    bool hits[3];
    for (int i = 0; i < 3; i++)
    {
        hits[i] = MoveAxis(player->camera.position, i, player->velocity[i] * dt);
    }
    player->is_on_ground = hits[1] && player->velocity[1] < 0.0f;
    for (int i = 0; i < 3; i++)
    {
        player->velocity[i] *= !hits[i];
    }
}

static void SetBlock(const Player* player)
{
    if (player->query.block == BLOCK_EMPTY)
    {
        return;
    }
    if (!Block_IsSolid(player->block))
    {
        World_SetBlock(player->query.previous, player->block);
        return;
    }
    for (int i = 0; i < 3; i++)
    {
        float min = player->camera.position[i] + AABB[0][i] + EPSILON;
        float max = player->camera.position[i] + AABB[1][i] - EPSILON;
        if (max <= player->query.previous[i] || min >= player->query.previous[i] + 1.0f)
        {
            World_SetBlock(player->query.previous, player->block);
            break;
        }
    }
}

void Player_Update(Player* player, float dt)
{
    if (Input_GetToggleController())
    {
        player->controller++;
        player->controller %= PLAYER_CONTROLLER_COUNT;
    }
    float pan[2];
    Input_GetRotation(pan);
    Camera_Rotate(&player->camera, pan[1] * -SENSITIVITY, pan[0] * SENSITIVITY);
    Move(player, dt);
    player->query = World_Raycast(&player->camera, REACH);
    int change_block = Input_GetChangeBlock();
    if (change_block)
    {
        static const int COUNT = BLOCK_COUNT - BLOCK_EMPTY - 1;
        int block = (player->block - (BLOCK_EMPTY + 1) + change_block) % COUNT;
        player->block = (block < 0 ? block + COUNT : block) + BLOCK_EMPTY + 1;
    }
    if (Input_GetSelectBlock() && player->query.block != BLOCK_EMPTY)
    {
        player->block = player->query.block;
    }
    if (Input_GetBreakBlock() && player->query.block != BLOCK_EMPTY)
    {
        World_SetBlock(player->query.current, BLOCK_EMPTY);
    }
    if (Input_GetPlaceBlock())
    {
        SetBlock(player);
    }
}
