#include <SDL3/SDL.h>

#include "map.h"

static const int kEmpty = 0;
static const int kTombstone = 255;
static const float kMaxLoad = 0.7f;

static bool Equals(const MapRow row, int x, int y, int z)
{
    return row.X == x && row.Y == y && row.Z == z;
}

static void Create(Map* map, Uint32 capacity)
{
    SDL_assert(SDL_HasExactlyOneBitSet32(capacity));
    map->Rows = SDL_calloc(capacity, sizeof(MapRow));
    map->Capacity = capacity;
    map->Size = 0;
}

void CreateMap(Map* map)
{
    Create(map, 32);
}

void DestroyMap(Map* map)
{
    SDL_free(map->Rows);
    map->Rows = NULL;
    map->Size = 0;
    map->Capacity = 0;
}

static int HashInt(int x)
{
    x += (x << 10);
    x ^= (x >> 6);
    x += (x << 3);
    x ^= (x >> 11);
    x += (x << 15);
    return x;
}

static int HashXYZ(int x, int y, int z)
{
    return HashInt(x) ^ HashInt(y) ^ HashInt(z);
}

static void grow(Map* map)
{
    Map old_map = *map;
    Create(map, old_map.Capacity * 2);
    for (Uint32 i = 0; i < old_map.Capacity; ++i)
    {
        if (IsMapRowValid(&old_map, i))
        {
            MapRow row = old_map.Rows[i];
            SetMapValue(map, row.X, row.Y, row.Z, row.Value);
        }
    }
    DestroyMap(&old_map);
}

void SetMapValue(Map* map, int x, int y, int z, int value)
{
    SDL_assert(value <= UINT8_MAX);
    SDL_assert(value != kEmpty && value != kTombstone);
    if ((float) (map->Size + 1) / map->Capacity > kMaxLoad)
    {
        grow(map);
    }
    Uint32 mask = map->Capacity - 1;
    Uint32 index = HashXYZ(x, y, z) & mask;
    Uint32 tombstone = UINT32_MAX;
    for (;;)
    {
        MapRow* row = &map->Rows[index];
        if (row->Value == kEmpty)
        {
            if (tombstone != UINT32_MAX)
            {
                row = &map->Rows[tombstone];
            }
            row->X = x;
            row->Y = y;
            row->Z = z;
            row->Value = value;
            map->Size++;
            return;
        }
        if (row->Value == kTombstone)
        {
            if (tombstone == UINT32_MAX)
            {
                tombstone = index;
            }
        }
        else if (Equals(*row, x, y, z))
        {
            row->Value = value;
            return;
        }
        index = (index + 1) & mask;
    }
}

int GetMapValue(const Map* map, int x, int y, int z)
{
    Uint32 mask = map->Capacity - 1;
    Uint32 index = HashXYZ(x, y, z) & mask;
    for (;;)
    {
        const MapRow row = map->Rows[index];
        if (row.Value == kEmpty)
        {
            return kEmpty;
        }
        if (row.Value != kTombstone && Equals(row, x, y, z))
        {
            return row.Value;
        }
        index = (index + 1) & mask;
    }
}

void RemoveMapValue(Map* map, int x, int y, int z)
{
    Uint32 mask = map->Capacity - 1;
    Uint32 index = HashXYZ(x, y, z) & mask;
    for (;;)
    {
        MapRow* row = &map->Rows[index];
        if (row->Value == kEmpty)
        {
            return;
        }
        if (row->Value != kTombstone && Equals(*row, x, y, z))
        {
            row->Value = kTombstone;
            map->Size--;
            return;
        }
        index = (index + 1) & mask;
    }
}

void ClearMap(Map* map)
{
    memset(map->Rows, 0,  map->Capacity * sizeof(MapRow));
    map->Size = 0;
}

bool IsMapRowValid(const Map* map, Uint32 index)
{
    MapRow row = map->Rows[index];
    return row.Value != kEmpty && row.Value != kTombstone;
}

MapRow GetMapRow(const Map* map, Uint32 index)
{
    SDL_assert(IsMapRowValid(map, index));
    return map->Rows[index];
}
