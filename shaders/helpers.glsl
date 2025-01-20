#ifndef HELPERS_GLSL
#define HELPERS_GLSL

#include "config.h"

const vec3 normals[6] = vec3[6]
(
    vec3( 0, 0, 1 ),
    vec3( 0, 0,-1 ),
    vec3( 1, 0, 0 ),
    vec3(-1, 0, 0 ),
    vec3( 0, 1, 0 ),
    vec3( 0,-1, 0 )
);

vec3 get_position(
    const uint voxel)
{
    return vec3(
        voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK,
        voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK,
        voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK);
}

vec2 get_atlas(
    const vec2 position)
{
    vec2 uv;
    uv.x = position.x / ATLAS_WIDTH * ATLAS_FACE_WIDTH;
    uv.y = position.y / ATLAS_HEIGHT * ATLAS_FACE_HEIGHT;
    return uv;
}

vec2 get_uv(
    const uint voxel)
{
    const vec2 uv = vec2(
        voxel >> VOXEL_U_OFFSET & VOXEL_U_MASK,
        voxel >> VOXEL_V_OFFSET & VOXEL_V_MASK);
    return get_atlas(uv);
}

uint get_direction(
    const uint voxel)
{
    return voxel >> VOXEL_DIRECTION_OFFSET & VOXEL_DIRECTION_MASK;
}

vec3 get_normal(
    const uint voxel)
{
    return normals[get_direction(voxel)];
}

bool get_shadow(
    const uint voxel)
{
    return bool(voxel >> VOXEL_SHADOW_OFFSET & VOXEL_SHADOW_MASK);
}

bool get_shadowed(
    const uint voxel)
{
    return bool(voxel >> VOXEL_SHADOWED_OFFSET & VOXEL_SHADOWED_MASK);
}

vec3 get_sky(
    const float y)
{
    return mix(vec3(0.3, 0.6, 0.9), vec3(0.8, 0.95, 1.0), clamp(y, 0.0, 0.8));
}

float get_fog(
    const float x)
{
    return min(pow(x / 250.0, 2.5), 1.0);
}

float get_random(
    const vec2 position)
{
    return fract(sin(dot(position, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 get_color(
    const vec4 color,
    const sampler2D shadowmap,
    const vec3 position,
    const vec2 uv,
    const vec3 normal,
    const vec3 player_position,
    const vec3 shadow_position,
    const vec3 shadow_vector,
    const bool shadowed,
    const float fog,
    const float ssao)
{
    vec3 shadow_uv;
    shadow_uv.x = shadow_position.x * 0.5 + 0.5;
    shadow_uv.y = 1.0 - (shadow_position.y * 0.5 + 0.5);
    shadow_uv.z = shadow_position.z;
    float a;
    float b;
    float c;
    const float angle = dot(normal, -shadow_vector);
    const float depth = shadow_uv.z - 0.001;
    if (shadowed && ((angle < 0.0) || (
        all(greaterThanEqual(shadow_uv, vec3(0.0))) &&
        all(lessThanEqual(shadow_uv, vec3(1.0))) &&
        (depth > texture(shadowmap, shadow_uv.xy).x))))
    {
        a = ssao * 0.2;
        b = 0.0;
        c = 0.0;
    }
    else
    {
        a = ssao * 0.3;
        b = 0.4;
        c = max(angle, 0.0) * 0.6;
    }
    const vec4 composite = vec4(color.xyz * (a + b + c + 0.3), color.a);
    const float dy = position.y - player_position.y;
    const float dx = distance(position.xz, player_position.xz);
    const float pitch = atan(dy, dx);
    const vec4 sky = vec4(get_sky(pitch), 1.0);
    return mix(composite, sky, fog);
}

#endif