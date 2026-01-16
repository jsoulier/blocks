#pragma once

typedef enum direction
{
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_COUNT,
}
direction_t;

static const int DIRECTIONS[DIRECTION_COUNT][3] =
{
    [DIRECTION_NORTH] = { 0, 0, 1 },
    [DIRECTION_SOUTH] = { 0, 0,-1 },
    [DIRECTION_EAST]  = { 1, 0, 0 },
    [DIRECTION_WEST]  = {-1, 0, 0 },
    [DIRECTION_UP]    = { 0, 1, 0 },
    [DIRECTION_DOWN]  = { 0,-1, 0 },
};
