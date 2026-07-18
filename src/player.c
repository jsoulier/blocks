#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
#include "player.h"
#include "save.h"
#include "world.h"

typedef struct AABB
{
    float min[3];
    float max[3];
} AABB;

typedef struct PlayerSave
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    Block block;
} PlayerSave;

static const float PHYSICS_EPSILON = 0.001f;
static const float WALK_SPEED = 5.0f;
static const float SPRINT_SPEED = 9.0f;
static const float SENSITIVITY = 0.1f;
static const float REACH = 10.0f;
static const float AIR_ACCELERATION = 6.0f;
static const float GRAVITY = 24.0f;
static const float JUMP_SPEED = 8.5f;
static const float FLY_SPEED = 0.01f;
static const float FLY_FAST_SPEED = 0.1f;
static const float COLLISION_STEP = 0.1f;
static const float GROUND_OFFSET = 0.002f;

static const AABB PLAYER_AABB = {{-0.3f, -1.62f, -0.3f}, {0.3f, 1.8f - 1.62f, 0.3f}};

static bool IsColliding(const float position[3])
{
    int min[3];
    int max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = SDL_floorf(position[i] + PLAYER_AABB.min[i] + PHYSICS_EPSILON);
        max[i] = SDL_floorf(position[i] + PLAYER_AABB.max[i] - PHYSICS_EPSILON);
    }
    for (int bx = min[0]; bx <= max[0]; bx++)
        for (int by = min[1]; by <= max[1]; by++)
            for (int bz = min[2]; bz <= max[2]; bz++)
            {
                int position[3] = {bx, by, bz};
                if (Block_IsSolid(World_GetBlock(position)))
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
        float t = (lower + upper) * 0.5f;
        float location[3] = {start[0], start[1], start[2]};
        location[axis] += step * t;
        if (IsColliding(location))
        {
            upper = t;
        }
        else
        {
            lower = t;
        }
    }
    position[axis] = start[axis] + step * lower;
}

static bool Move(float position[3], int axis, float delta)
{
    if (SDL_fabsf(delta) <= SDL_FLT_EPSILON)
    {
        return false;
    }
    int steps = SDL_ceilf(SDL_fabsf(delta) / COLLISION_STEP);
    steps = SDL_max(steps, 1);
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
        SDL_memcpy(position, location, 12);
    }
    return false;
}

void Player_Load(Player* player)
{
    PlayerSave data;
    Camera_Init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player->camera.x = -200.0f;
    player->camera.y = 50.0f;
    player->camera.z = 0.0f;
    player->controller = PLAYER_CONTROLLER_WALK;
    player->block = BLOCK_YELLOW_TORCH;
    if (Save_GetPlayer(&data, sizeof(data)))
    {
        player->block = data.block;
        player->camera.x = data.x;
        player->camera.y = data.y;
        player->camera.z = data.z;
        player->camera.pitch = data.pitch;
        player->camera.yaw = data.yaw;
    }
    player->query = World_Raycast(&player->camera, REACH);
}

void Player_Save(const Player* player)
{
    PlayerSave data;
    data.x = player->camera.x;
    data.y = player->camera.y;
    data.z = player->camera.z;
    data.pitch = player->camera.pitch;
    data.yaw = player->camera.yaw;
    data.block = player->block;
    Save_SetPlayer(&data, sizeof(data));
}

void Player_ToggleController(Player* player)
{
    player->controller++;
    player->controller %= PLAYER_CONTROLLER_COUNT;
}

void Player_Rotate(Player* player, float pitch, float yaw)
{
    Camera_Rotate(&player->camera, pitch * -SENSITIVITY, yaw * SENSITIVITY);
    player->query = World_Raycast(&player->camera, REACH);
}

void Player_Move(Player* player, float dt)
{
    const bool* keys = SDL_GetKeyboardState(NULL);
    if (player->controller == PLAYER_CONTROLLER_WALK)
    {
        dt = SDL_min(dt * 0.001f, 0.05f);
        float input_x = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float input_z = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
        float length = SDL_sqrtf(input_x * input_x + input_z * input_z);
        if (length > SDL_FLT_EPSILON)
        {
            input_x /= length;
            input_z /= length;
        }
        float speed = keys[SDL_SCANCODE_LCTRL] ? SPRINT_SPEED : WALK_SPEED;
        float sy = SDL_sinf(player->camera.yaw);
        float cy = SDL_cosf(player->camera.yaw);
        float target_x = (cy * input_x + sy * input_z) * speed;
        float target_z = (sy * input_x - cy * input_z) * speed;
        if (player->is_on_ground)
        {
            player->velocity[0] = target_x;
            player->velocity[2] = target_z;
        }
        else
        {
            float blend = SDL_min(1.0f, AIR_ACCELERATION * dt);
            player->velocity[0] += (target_x - player->velocity[0]) * blend;
            player->velocity[2] += (target_z - player->velocity[2]) * blend;
        }
        if (keys[SDL_SCANCODE_SPACE] && player->is_on_ground)
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
            hits[i] = Move(player->camera.position, i, player->velocity[i] * dt);
        }
        if (hits[0])
        {
            player->velocity[0] = 0.0f;
        }
        if (hits[2])
        {
            player->velocity[2] = 0.0f;
        }
        if (hits[1])
        {
            if (player->velocity[1] < 0.0f)
            {
                player->is_on_ground = true;
            }
            player->velocity[1] = 0.0f;
        }
        else
        {
            player->is_on_ground = false;
        }
    }
    else
    {
        float speed = keys[SDL_SCANCODE_LCTRL] ? FLY_FAST_SPEED : FLY_SPEED;
        float dx = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float dy = (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE]) - (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LSHIFT]);
        float dz = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
        Camera_Move(&player->camera, dx * speed * dt, dy * speed * dt, dz * speed * dt);
    }
    player->query = World_Raycast(&player->camera, REACH);
}

void Player_PlaceBlock(const Player* player)
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
        float min = player->camera.position[i] + PLAYER_AABB.min[i] + PHYSICS_EPSILON;
        float max = player->camera.position[i] + PLAYER_AABB.max[i] - PHYSICS_EPSILON;
        if (max <= player->query.previous[i] || min >= player->query.previous[i] + 1.0f)
        {
            World_SetBlock(player->query.previous, player->block);
            break;
        }
    }
}

void Player_SelectBlock(Player* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        player->block = player->query.block;
    }
}

void Player_BreakBlock(const Player* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        World_SetBlock(player->query.current, BLOCK_EMPTY);
    }
}

void Player_ChangeBlock(Player* player, int dy)
{
    static const int COUNT = BLOCK_COUNT - BLOCK_EMPTY - 1;
    int block = player->block - (BLOCK_EMPTY + 1) + dy;
    block = (block + COUNT) % COUNT;
    player->block = block + BLOCK_EMPTY + 1;
}
