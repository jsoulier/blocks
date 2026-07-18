#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
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
    {
        for (int by = min[1]; by <= max[1]; by++)
        {
            for (int bz = min[2]; bz <= max[2]; bz++)
            {
                int block[3] = {bx, by, bz};
                if (Block_IsSolid(World_GetBlock(block)))
                {
                    return true;
                }
            }
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

static bool Move(float position[3], int axis, float delta)
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
    player->query = World_Raycast(&player->camera, REACH);
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
        float right = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float forward = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
        float length = SDL_sqrtf(right * right + forward * forward);
        if (length > SDL_FLT_EPSILON)
        {
            right /= length;
            forward /= length;
        }
        float speed = WALK_SPEED * (keys[SDL_SCANCODE_LCTRL] ? SPRINT_MULTIPLER : 1.0f);
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
            float blend = SDL_min(1.0f, AIR_ACCELERATION * dt);
            player->velocity[0] += (dx - player->velocity[0]) * blend;
            player->velocity[2] += (dz - player->velocity[2]) * blend;
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
        player->is_on_ground = hits[1] && player->velocity[1] < 0.0f;
        for (int i = 0; i < 3; i++)
        {
            player->velocity[i] *= !hits[i];
        }
    }
    else
    {
        float speed = FLY_SPEED * (keys[SDL_SCANCODE_LCTRL] ? SPRINT_MULTIPLER : 1.0f);
        float dx = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float dy = keys[SDL_SCANCODE_SPACE] + keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_LSHIFT] - keys[SDL_SCANCODE_Q];
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
        float min = player->camera.position[i] + AABB[0][i] + EPSILON;
        float max = player->camera.position[i] + AABB[1][i] - EPSILON;
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
