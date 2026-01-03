#pragma once

#include <stdint.h>

typedef struct light
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t intensity;
}
light_t;