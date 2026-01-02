#pragma once

#include <stdint.h>

typedef struct map_row map_row_t;
typedef struct map
{
    map_row_t* rows;
    uint32_t size;
    uint32_t capacity;
}
map_t;

void map_init(map_t* map);
void map_free(map_t* map);
void map_set(map_t* map, int x, int y, int z, int value);
int map_get(const map_t* map, int x, int y, int z);
void map_remove(map_t* map, int x, int y, int z);
bool map_is_valid(const map_t* map, uint32_t index);