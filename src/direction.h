#pragma once

typedef enum Direction
{
    DirectionNorth,
    DirectionSouth,
    DirectionEast,
    DirectionWest,
    DirectionUp,
    DirectionDown,
}
Direction;

static const int kDirections[][3] =
{
    [DirectionNorth] = { 0, 0, 1 },
    [DirectionSouth] = { 0, 0,-1 },
    [DirectionEast]  = { 1, 0, 0 },
    [DirectionWest]  = {-1, 0, 0 },
    [DirectionUp]    = { 0, 1, 0 },
    [DirectionDown]  = { 0,-1, 0 },
};
