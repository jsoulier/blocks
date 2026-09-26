#include <SDL3/SDL.h>

#include "map.h"

static const int EMPTY = 0;
static const int TOMBSTONE = 255;
static const float MAX_LOAD_FACTOR = 0.75f;

static Uint32 HashInt(Uint32 x)
{
    x += (x << 10);
    x ^= (x >> 6);
    x += (x << 3);
    x ^= (x >> 11);
    x += (x << 15);
    return x;
}

static Uint32 HashPosition(Uint32 x, Uint32 y, Uint32 z)
{
    return HashInt(x) ^ HashInt(y) ^ HashInt(z);
}

static bool IsEqual(MapRow row, int x, int y, int z)
{
    return row.x == x && row.y == y && row.z == z;
}

static void Grow(Map* map)
{
    Map old_map = *map;
    Map_Init(map, old_map.capacity * 2);
    for (Uint32 i = 0; i < old_map.capacity; i++)
    {
        if (Map_IsRowValid(&old_map, i))
        {
            MapRow row = old_map.rows[i];
            Map_Set(map, row.x, row.y, row.z, row.value);
        }
    }
    Map_Free(&old_map);
}

void Map_Init(Map* map, int capacity)
{
    SDL_assert(SDL_HasExactlyOneBitSet32(capacity));
    map->rows = SDL_calloc(capacity, sizeof(MapRow));
    map->capacity = capacity;
    map->size = 0;
}

void Map_Free(Map* map)
{
    SDL_free(map->rows);
    map->rows = NULL;
    map->size = 0;
    map->capacity = 0;
}

void Map_Set(Map* map, int x, int y, int z, int value)
{
    SDL_assert(value <= SDL_MAX_UINT8);
    SDL_assert(value != EMPTY && value != TOMBSTONE);
    if ((float) (map->size + 1) / map->capacity > MAX_LOAD_FACTOR)
    {
        Grow(map);
    }
    Uint32 mask = map->capacity - 1;
    Uint32 start = HashPosition(x, y, z) & mask;
    Uint32 index = start;
    Uint32 tombstone = SDL_MAX_UINT32;
    MapRow* row = NULL;
    for (;;)
    {
        row = &map->rows[index];
        if (row->value == EMPTY)
        {
            if (tombstone != SDL_MAX_UINT32)
            {
                row = &map->rows[tombstone];
            }
            break;
        }
        if (row->value == TOMBSTONE)
        {
            if (tombstone == SDL_MAX_UINT32)
            {
                tombstone = index;
            }
        }
        else if (IsEqual(*row, x, y, z))
        {
            row->value = value;
            return;
        }
        index = (index + 1) & mask;
        if (index == start)
        {
            SDL_assert(tombstone != SDL_MAX_UINT32);
            row = &map->rows[tombstone];
            break;
        }
    }
    SDL_assert(row);
    row->x = x;
    row->y = y;
    row->z = z;
    row->value = value;
    map->size++;
}

int Map_Get(const Map* map, int x, int y, int z)
{
    Uint32 mask = map->capacity - 1;
    Uint32 start = HashPosition(x, y, z) & mask;
    Uint32 index = start;
    for (;;)
    {
        const MapRow row = map->rows[index];
        if (row.value == EMPTY)
        {
            return EMPTY;
        }
        if (row.value != TOMBSTONE && IsEqual(row, x, y, z))
        {
            return row.value;
        }
        index = (index + 1) & mask;
        if (index == start)
        {
            return EMPTY;
        }
    }
}

void Map_Remove(Map* map, int x, int y, int z)
{
    Uint32 mask = map->capacity - 1;
    Uint32 start = HashPosition(x, y, z) & mask;
    Uint32 index = start;
    for (;;)
    {
        MapRow* row = &map->rows[index];
        if (row->value == EMPTY)
        {
            return;
        }
        if (row->value != TOMBSTONE && IsEqual(*row, x, y, z))
        {
            row->value = TOMBSTONE;
            map->size--;
            return;
        }
        index = (index + 1) & mask;
        if (index == start)
        {
            return;
        }
    }
}

void Map_Clear(Map* map)
{
    SDL_memset(map->rows, 0, map->capacity * sizeof(MapRow));
    map->size = 0;
}

bool Map_IsRowValid(const Map* map, Uint32 index)
{
    MapRow row = map->rows[index];
    return row.value != EMPTY && row.value != TOMBSTONE;
}

MapRow Map_GetRow(const Map* map, Uint32 index)
{
    SDL_assert(Map_IsRowValid(map, index));
    return map->rows[index];
}
