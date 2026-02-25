#include <SDL3/SDL.h>

#include "block.h"
#include "camera.h"
#include "physics.h"
#include "player.h"
#include "world.h"

static const float MOVE_SPEED = 5.0f;
static const float SPRINT_MULTIPLIER = 1.8f;
static const float AIR_ACCEL = 6.0f;
static const float GRAVITY = 24.0f;
static const float JUMP_SPEED = 8.5f;
static const float FREECAM_SPEED = 0.01f;
static const float FREECAM_FAST_MULTIPLIER = 10.0f;
static const float COLLISION_STEP = 0.1f;
static const float GROUND_CHECK_OFFSET = 0.002f;
static const float PLAYER_COLLISION_RADIUS = 0.3f;
static const float PLAYER_COLLISION_HEIGHT = 1.8f;
static const float PLAYER_EYE_OFFSET = 1.62f;

static physics_aabb_t player_aabb(void) {
  return (physics_aabb_t){
      .min_x = -PLAYER_COLLISION_RADIUS,
      .max_x = PLAYER_COLLISION_RADIUS,
      .min_y = -PLAYER_EYE_OFFSET,
      .max_y = PLAYER_COLLISION_HEIGHT - PLAYER_EYE_OFFSET,
      .min_z = -PLAYER_COLLISION_RADIUS,
      .max_z = PLAYER_COLLISION_RADIUS,
  };
}

static bool is_block_solid(int x, int y, int z) {
  int position[3] = {x, y, z};
  return block_is_solid(world_get_block(position));
}

static void move_physics(player_t *player, float dt_ms, const bool *state) {
  const physics_aabb_t aabb = player_aabb();
  float dt = SDL_min(dt_ms * 0.001f, 0.05f);
  float input_x = state[SDL_SCANCODE_D] - state[SDL_SCANCODE_A];
  float input_z = state[SDL_SCANCODE_W] - state[SDL_SCANCODE_S];
  float length = SDL_sqrtf(input_x * input_x + input_z * input_z);
  if (length > SDL_FLT_EPSILON) {
    input_x /= length;
    input_z /= length;
  }

  float speed = MOVE_SPEED;
  if (state[SDL_SCANCODE_LCTRL]) {
    speed *= SPRINT_MULTIPLIER;
  }

  float sy = SDL_sinf(player->camera.yaw);
  float cy = SDL_cosf(player->camera.yaw);
  float target_x = (cy * input_x + sy * input_z) * speed;
  float target_z = (sy * input_x - cy * input_z) * speed;

  if (player->on_ground) {
    player->velocity[0] = target_x;
    player->velocity[2] = target_z;
  } else {
    float blend = SDL_min(1.0f, AIR_ACCEL * dt);
    player->velocity[0] += (target_x - player->velocity[0]) * blend;
    player->velocity[2] += (target_z - player->velocity[2]) * blend;
  }

  bool jump_down = state[SDL_SCANCODE_SPACE];
  if (jump_down && !player->jump_was_down && player->on_ground) {
    player->velocity[1] = JUMP_SPEED;
    player->on_ground = false;
  }
  player->jump_was_down = jump_down;

  // Before anything "time" check delta for early return (another guard against precision issues/jitter)
  // we still need to process input up to this point tho.
  if (dt <= SDL_FLT_EPSILON) {
    return;
  }

  player->velocity[1] -= GRAVITY * dt;
  float x = player->camera.x;
  float y = player->camera.y;
  float z = player->camera.z;

  bool hit_x = physics_move_axis(&aabb, &x, &y, &z, 0, player->velocity[0] * dt, COLLISION_STEP, is_block_solid);
  bool hit_y = physics_move_axis(&aabb, &x, &y, &z, 1, player->velocity[1] * dt, COLLISION_STEP, is_block_solid);
  bool hit_z = physics_move_axis(&aabb, &x, &y, &z, 2, player->velocity[2] * dt, COLLISION_STEP, is_block_solid);

  player->camera.x = x;
  player->camera.y = y;
  player->camera.z = z;

  if (hit_x) {
    player->velocity[0] = 0.0f;
  }
  if (hit_z) {
    player->velocity[2] = 0.0f;
  }
  if (hit_y) {
    if (player->velocity[1] < 0.0f) {
      player->on_ground = true;
    }
    player->velocity[1] = 0.0f;
  } else {
    player->on_ground = false;
  }
}

static void move_freecam(player_t *player, float dt_ms, const bool *state) {
  float speed = FREECAM_SPEED;
  float dx = state[SDL_SCANCODE_D] - state[SDL_SCANCODE_A];
  float dy = (state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE]) -
             (state[SDL_SCANCODE_Q] || state[SDL_SCANCODE_LSHIFT]);
  float dz = state[SDL_SCANCODE_W] - state[SDL_SCANCODE_S];
  if (state[SDL_SCANCODE_LCTRL]) {
    speed *= FREECAM_FAST_MULTIPLIER;
  }
  camera_move(&player->camera, dx * speed * dt_ms, dy * speed * dt_ms,
              dz * speed * dt_ms);
  SDL_zerop(player->velocity);
  player->jump_was_down = state[SDL_SCANCODE_SPACE];
  player->on_ground = false;
}

void player_init(player_t *player) {
  SDL_zerop(player);
  camera_init(&player->camera, CAMERA_TYPE_PERSPECTIVE);
  player->camera.x = -200.0f;
  player->camera.y = 50.0f;
  player->camera.z = 0.0f;
  player->block = BLOCK_YELLOW_TORCH;
  player->controller = PLAYER_CONTROLLER_FP;
}

void player_set_controller(player_t *player, player_controller_t controller) {
  if (player->controller == controller) {
    return;
  }
  player->controller = controller;
  SDL_zerop(player->velocity);
  player->jump_was_down = false;
  if (controller == PLAYER_CONTROLLER_FP) {
    player_update_grounded(player);
  } else {
    player->on_ground = false;
  }
}

void player_toggle_controller(player_t *player) {
  if (player->controller == PLAYER_CONTROLLER_FP) {
    player_set_controller(player, PLAYER_CONTROLLER_FREECAM);
  } else {
    player_set_controller(player, PLAYER_CONTROLLER_FP);
  }
}

const char *player_controller_name(player_controller_t controller) {
  if (controller == PLAYER_CONTROLLER_FREECAM) {
    return "freecam";
  } else {
    return "first_person";
  }
}

void player_rotate(player_t *player, float pitch, float yaw,
                   float sensitivity) {
  camera_rotate(&player->camera, pitch * -sensitivity, yaw * sensitivity);
}

void player_move(player_t *player, float dt_ms, const bool *keyboard_state) {
  if (player->controller == PLAYER_CONTROLLER_FREECAM) {
    move_freecam(player, dt_ms, keyboard_state);
  } else {
    move_physics(player, dt_ms, keyboard_state);
  }
}

bool player_overlaps_block(const player_t *player, const int position[3]) {
  const physics_aabb_t aabb = player_aabb();
  return physics_overlaps_block(&aabb, player->camera.x,
                                player->camera.y, player->camera.z, position);
}

void player_update_grounded(player_t *player) {
  const physics_aabb_t aabb = player_aabb();
  player->on_ground = physics_is_colliding(
      &aabb, player->camera.x, player->camera.y - GROUND_CHECK_OFFSET,
      player->camera.z, is_block_solid);
}
