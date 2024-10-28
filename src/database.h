#pragma once

#include <stdbool.h>
#include "helpers.h"

bool database_init(const char* file);
void database_free();
void database_commit();
void database_set_player(
    const int id,
    const float x,
    const float y,
    const float z,
    const float pitch,
    const float yaw);
bool database_get_player(
    const int id,
    float* x,
    float* y,
    float* z,
    float* pitch,
    float* yaw);