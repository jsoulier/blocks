#include <SDL3/SDL.h>

#include "check.h"
#include "map.h"

static const int EMPTY = 0;
static const int TOMBSTONE = 255;
static const float MAX_LOAD = 0.7f;

static bool is_equal(const map_row_t row, int x, int y, int z)
{
    return row.x == x && row.y == y && row.z == z;
}

void map_init(map_t* map, int capacity)
{
    CHECK(SDL_HasExactlyOneBitSet32(capacity));
    map->rows = SDL_calloc(capacity, sizeof(map_row_t));
    map->capacity = capacity;
    map->size = 0;
}

void map_free(map_t* map)
{
    SDL_free(map->rows);
    map->rows = NULL;
    map->size = 0;
    map->capacity = 0;
}

static int hash_int(int x)
{
    x += (x << 10);
    x ^= (x >> 6);
    x += (x << 3);
    x ^= (x >> 11);
    x += (x << 15);
    return x;
}

static int hash_xyz(int x, int y, int z)
{
    return hash_int(x) ^ hash_int(y) ^ hash_int(z);
}

static void grow(map_t* map)
{
    map_t old_map = *map;
    map_init(map, old_map.capacity * 2);
    for (Uint32 i = 0; i < old_map.capacity; ++i)
    {
        if (map_is_row_valid(&old_map, i))
        {
            map_row_t row = old_map.rows[i];
            map_set(map, row.x, row.y, row.z, row.value);
        }
    }
    map_free(&old_map);
}

void map_set(map_t* map, int x, int y, int z, int value)
{
    CHECK(value <= UINT8_MAX);
    CHECK(value != EMPTY && value != TOMBSTONE);
    if ((float) (map->size + 1) / map->capacity > MAX_LOAD)
    {
        grow(map);
    }
    Uint32 mask = map->capacity - 1;
    Uint32 index = hash_xyz(x, y, z) & mask;
    Uint32 tombstone = SDL_MAX_UINT32;
    for (;;)
    {
        map_row_t* row = &map->rows[index];
        if (row->value == EMPTY)
        {
            if (tombstone != SDL_MAX_UINT32)
            {
                row = &map->rows[tombstone];
            }
            row->x = x;
            row->y = y;
            row->z = z;
            row->value = value;
            map->size++;
            return;
        }
        if (row->value == TOMBSTONE)
        {
            if (tombstone == SDL_MAX_UINT32)
            {
                tombstone = index;
            }
        }
        else if (is_equal(*row, x, y, z))
        {
            row->value = value;
            return;
        }
        index = (index + 1) & mask;
    }
}

int map_get(const map_t* map, int x, int y, int z)
{
    Uint32 mask = map->capacity - 1;
    Uint32 index = hash_xyz(x, y, z) & mask;
    for (;;)
    {
        const map_row_t row = map->rows[index];
        if (row.value == EMPTY)
        {
            return EMPTY;
        }
        if (row.value != TOMBSTONE && is_equal(row, x, y, z))
        {
            return row.value;
        }
        index = (index + 1) & mask;
    }
}

void map_remove(map_t* map, int x, int y, int z)
{
    Uint32 mask = map->capacity - 1;
    Uint32 index = hash_xyz(x, y, z) & mask;
    for (;;)
    {
        map_row_t* row = &map->rows[index];
        if (row->value == EMPTY)
        {
            return;
        }
        if (row->value != TOMBSTONE && is_equal(*row, x, y, z))
        {
            row->value = TOMBSTONE;
            map->size--;
            return;
        }
        index = (index + 1) & mask;
    }
}

void map_clear(map_t* map)
{
    memset(map->rows, 0,  map->capacity * sizeof(map_row_t));
    map->size = 0;
}

bool map_is_row_valid(const map_t* map, Uint32 index)
{
    map_row_t row = map->rows[index];
    return row.value != EMPTY && row.value != TOMBSTONE;
}

map_row_t map_get_row(const map_t* map, Uint32 index)
{
    CHECK(map_is_row_valid(map, index));
    return map->rows[index];
}
