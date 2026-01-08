#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef struct chunk chunk_t;
typedef struct player player_t;

bool save_init(const char* path);
void save_free();
void save_commit();
void save_set_player(const player_t* player);
bool save_get_player(player_t* player);
void save_set_block(const chunk_t* chunk, int x, int y, int z, block_t block);
void save_get_chunk(chunk_t* chunk);
