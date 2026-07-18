#pragma once

typedef enum Direction
{
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST,
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_COUNT,
} Direction;

static const int DIRECTIONS[DIRECTION_COUNT][3] = {
    [DIRECTION_NORTH] = {0, 0, 1},  // +Z
    [DIRECTION_SOUTH] = {0, 0, -1}, // -Z
    [DIRECTION_EAST] = {1, 0, 0},   // +X
    [DIRECTION_WEST] = {-1, 0, 0},  // -X
    [DIRECTION_UP] = {0, 1, 0},     // +Y
    [DIRECTION_DOWN] = {0, -1, 0},  // -Y
};
