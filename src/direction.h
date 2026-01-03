#pragma once

typedef enum direction
{
    DIRECTION_N,
    DIRECTION_S,
    DIRECTION_E,
    DIRECTION_W,
    DIRECTION_U,
    DIRECTION_D,
    DIRECTION_2 = 4,
    DIRECTION_3 = 6,
}
direction_t;

static const int directions[][3] =
{
    [DIRECTION_N] = { 0, 0, 1 },
    [DIRECTION_S] = { 0, 0,-1 },
    [DIRECTION_E] = { 1, 0, 0 },
    [DIRECTION_W] = {-1, 0, 0 },
    [DIRECTION_U] = { 0, 1, 0 },
    [DIRECTION_D] = { 0,-1, 0 },
};

// TODO:
// static const int directions_2d[][2] =
// {
//     { 1, 1 },
//     { 1, 0 },
//     { 1,-1 },
//     { 0,-1 },
//     {-1,-1 },
//     {-1, 0 },
//     {-1, 1 },
//     { 0, 1 },
// };
