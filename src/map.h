#pragma once

#include <SDL3/SDL.h>

typedef struct MapRow
{
    Uint8 x;
    Uint8 y;
    Uint8 z;
    Uint8 value;
} MapRow;

typedef struct Map
{
    MapRow* rows;
    Uint32 size;
    Uint32 capacity;
} Map;

void Map_Init(Map* map, int capacity);
void Map_Free(Map* map);
void Map_Set(Map* map, int x, int y, int z, int value);
int Map_Get(const Map* map, int x, int y, int z);
void Map_Remove(Map* map, int x, int y, int z);
void Map_Clear(Map* map);
bool Map_IsRowValid(const Map* map, Uint32 index);
MapRow Map_GetRow(const Map* map, Uint32 index);
