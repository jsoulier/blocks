#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef void (*save_set_block_t)(void* userdata, int bx, int by, int bz, block_t block);

bool save_init(const char* path);
void save_free();
void save_commit();
void save_set_player(int id, const void* data, int size);
bool save_get_player(int id, void* data, int size);
void save_set_block(int cx, int cz, int bx, int by, int bz, block_t block);
void save_get_blocks(void* userdata, int cx, int cz, save_set_block_t function);
