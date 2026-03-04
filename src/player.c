#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
#include "player.h"
#include "save.h"
#include "world.h"

typedef struct aabb
{
    float min[3];
    float max[3];
}
aabb_t;

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
static const float COLLISION_RADIUS = 0.3f;
static const float COLLISION_HEIGHT = 1.8f;
static const float EYE_OFFSET = 1.62f;

static aabb_t get_aabb()
{
    return (aabb_t) {{-COLLISION_RADIUS, -EYE_OFFSET, -COLLISION_RADIUS},
        {COLLISION_RADIUS, COLLISION_HEIGHT - EYE_OFFSET, COLLISION_RADIUS}};
}

static bool is_solid(const float position[3])
{
    int index[3] = {position[0], position[1], position[2]};
    return block_is_solid(world_get_block(index));
}

static bool is_colliding(const aabb_t *aabb, const float position[3])
{
    int min[3];
    int max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = SDL_floorf(position[i] + aabb->min[i] + PHYSICS_EPSILON);
        max[i] = SDL_floorf(position[i] + aabb->max[i] - PHYSICS_EPSILON);
    }
    for (int bx = min[0]; bx <= max[0]; bx++)
    for (int by = min[1]; by <= max[1]; by++)
    for (int bz = min[2]; bz <= max[2]; bz++)
    {
        float location[3] = {bx, by, bz};
        if (is_solid(location))
        {
            return true;
        }
    }
    return false;
}

static void bisect(const aabb_t* aabb, float position[3], int axis, float step)
{
    float start[3] = {position[0], position[1], position[2]};
    float lower = 0.0f;
    float upper = 1.0f;
    for (int i = 0; i < 8; i++)
    {
        float t = (lower + upper) * 0.5f;
        float location[3] = {start[0], start[1], start[2]};
        location[axis] += step * t;
        if (is_colliding(aabb, location))
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

static bool move(const aabb_t* aabb, float position[3], int axis, float delta)
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
        if (is_colliding(aabb, location))
        {
            bisect(aabb, position, axis, step);
            return true;
        }
        SDL_memcpy(position, location, 12);
    }
    return false;
}

void player_save_or_load(player_t* player, int id, bool save)
{
    struct
    {
        float x;
        float y;
        float z;
        float pitch;
        float yaw;
        block_t block;
    }
    data;
    if (save)
    {
        data.x = player->camera.x;
        data.y = player->camera.y;
        data.z = player->camera.z;
        data.pitch = player->camera.pitch;
        data.yaw = player->camera.yaw;
        data.block = player->block;
        save_set_player(id, &data, sizeof(data));
        return;
    }
    camera_init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
    player->camera.x = -200.0f;
    player->camera.y = 50.0f;
    player->camera.z = 0.0f;
    player->controller = PLAYER_CONTROLLER_WALK;
    player->block = BLOCK_YELLOW_TORCH;
    if (save_get_player(id, &data, sizeof(data)))
    {
        player->block = data.block;
        player->camera.x = data.x;
        player->camera.y = data.y;
        player->camera.z = data.z;
        player->camera.pitch = data.pitch;
        player->camera.yaw = data.yaw;
    }
    player->query = world_raycast(&player->camera, REACH);
}

void player_toggle_controller(player_t* player)
{
    player->controller++;
    player->controller %= PLAYER_CONTROLLER_COUNT;
}

void player_rotate(player_t* player, float pitch, float yaw)
{
    camera_rotate(&player->camera, pitch * -SENSITIVITY, yaw * SENSITIVITY);
    player->query = world_raycast(&player->camera, REACH);
}

void player_move(player_t* player, float dt)
{
    const bool* keys = SDL_GetKeyboardState(NULL);
    if (player->controller == PLAYER_CONTROLLER_WALK)
    {
        const aabb_t aabb = get_aabb();
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
            hits[i] = move(&aabb, player->camera.position, i, player->velocity[i] * dt);
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
        player->query = world_raycast(&player->camera, REACH);
    }
    else
    {
        float speed = keys[SDL_SCANCODE_LCTRL] ? FLY_FAST_SPEED : FLY_SPEED;
        float dx = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
        float dy = (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_SPACE]) - (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LSHIFT]);
        float dz = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
        camera_move(&player->camera, dx * speed * dt, dy * speed * dt, dz * speed * dt);
    }
}

void player_place_block(const player_t* player)
{
    if (player->query.block == BLOCK_EMPTY)
    {
        return;
    }
    if (!block_is_solid(player->block))
    {
        world_set_block(player->query.previous, player->block);
        return;
    }
    const aabb_t aabb = get_aabb();
    for (int i = 0; i < 3; i++)
    {
        float min = player->camera.position[i] + aabb.min[i] + PHYSICS_EPSILON;
        float max = player->camera.position[i] + aabb.max[i] - PHYSICS_EPSILON;
        if (max <= player->query.previous[i] || min >= player->query.previous[i] + 1.0f)
        {
            world_set_block(player->query.previous, player->block);
            break;
        }
    }
}

void player_select_block(player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        player->block = player->query.block;
    }
}

void player_break_block(const player_t* player)
{
    if (player->query.block != BLOCK_EMPTY)
    {
        world_set_block(player->query.current, BLOCK_EMPTY);
    }
}

void player_change_block(player_t* player, int dy)
{
    static const int COUNT = BLOCK_COUNT - BLOCK_EMPTY - 1;
    int block = player->block - (BLOCK_EMPTY + 1) + dy;
    block = (block + COUNT) % COUNT;
    player->block = block + BLOCK_EMPTY + 1;
}
