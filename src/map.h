#pragma once

#include <SDL3/SDL.h>

typedef struct MapRow
{
    Uint8 X;
    Uint8 Y;
    Uint8 Z;
    Uint8 Value;
}
MapRow;

typedef struct Map
{
    MapRow* Rows;
    Uint32 Size;
    Uint32 Capacity;
}
Map;

void CreateMap(Map* map);
void DestroyMap(Map* map);
void SetMapValue(Map* map, int x, int y, int z, int value);
int GetMapValue(const Map* map, int x, int y, int z);
void RemoveMapValue(Map* map, int x, int y, int z);
void ClearMap(Map* map);
bool IsMapRowValid(const Map* map, Uint32 index);
MapRow GetMapRow(const Map* map, Uint32 index);
