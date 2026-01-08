#pragma once

#include <SDL3/SDL.h>

typedef struct map_row
{
    Uint8 x;
    Uint8 y;
    Uint8 z;
    Uint8 value;
}
map_row_t;

typedef struct map
{
    map_row_t* rows;
    Uint32 size;
    Uint32 capacity;
}
map_t;

void map_init(map_t* map, int capacity);
void map_free(map_t* map);
void map_set(map_t* map, int x, int y, int z, int value);
int map_get(const map_t* map, int x, int y, int z);
void map_remove(map_t* map, int x, int y, int z);
void map_clear(map_t* map);
bool map_is_row_valid(const map_t* map, Uint32 index);
map_row_t map_get_row(const map_t* map, Uint32 index);
