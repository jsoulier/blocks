#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct map_row
{
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t value;
}
map_row_t;

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
void map_clear(map_t* map);
bool map_is_valid(const map_t* map, uint32_t index);
map_row_t map_get_row(const map_t* map, uint32_t index);
