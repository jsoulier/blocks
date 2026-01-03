#pragma once

#include <stdint.h>

typedef struct chunk chunk_t;

typedef enum noise_type
{
    NOISE_TYPE_DEFAULT,
}
noise_type_t;

typedef struct noise
{
    noise_type_t type;
    uint32_t seed;
}
noise_t;

void noise_init(noise_t* noise, noise_type_t type, uint32_t seed);
void noise_generate(const noise_t* noise, chunk_t* chunk);