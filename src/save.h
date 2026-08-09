#pragma once

#include <SDL3/SDL.h>

#include "block.h"

typedef void (*SaveSetBlock)(void* userdata, int bx, int by, int bz, Block block);

bool Save_Init();
void Save_Free();
void Save_Commit();
void Save_SetPlayer(const void* data, int size);
bool Save_GetPlayer(void* data, int size);
void Save_SetSky(float time_of_day);
bool Save_GetSky(float* time_of_day);
void Save_SetBlock(int cx, int cz, int bx, int by, int bz, Block block);
void Save_GetBlocks(void* userdata, int cx, int cz, SaveSetBlock callback);
