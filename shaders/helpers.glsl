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

vec3 get_uv(
    const uint voxel)
{
    return vec3(
        voxel >> VOXEL_U_OFFSET & VOXEL_U_MASK,
        voxel >> VOXEL_V_OFFSET & VOXEL_V_MASK,
        voxel >> VOXEL_FACE_OFFSET & VOXEL_FACE_MASK);
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

bool get_occluded(
    const uint voxel)
{
    return bool(voxel >> VOXEL_OCCLUDED_OFFSET & VOXEL_OCCLUDED_MASK);
}

vec3 get_sky(
    const float y)
{
    return mix(vec3(0.7, 0.9, 1.0), vec3(0.3, 0.6, 0.9), clamp(y, 0.0, 0.8));
}

float get_fog(
    const float x)
{
    return min(pow(x / 250.0, 2.5), 1.0);
}

vec4 get_color(
    const sampler2DArray atlas,
    const sampler2D shadowmap,
    const vec3 position,
    const vec3 uv,
    const vec3 normal,
    const vec3 player_position,
    const vec3 shadow_position,
    const vec3 shadow_vector,
    const bool shadowed,
    const bool occluded,
    const float fog,
    const float ssao,
    const float alpha)
{
    vec3 shadow_uv;
    shadow_uv.x = shadow_position.x * 0.5 + 0.5;
    shadow_uv.y = 1.0 - (shadow_position.y * 0.5 + 0.5);
    shadow_uv.z = shadow_position.z;
    float ao = ssao * 0.4;
    float ambient = 0.2;
    float directional = 0.0;
    const float angle = dot(normal, -shadow_vector);
    const float depth = shadow_uv.z - 0.001;
    if (!shadowed || (angle > 0.0 && (
        all(lessThanEqual(shadow_uv, vec3(0.0))) ||
        all(greaterThanEqual(shadow_uv, vec3(1.0))) ||
        (depth < texture(shadowmap, shadow_uv.xy).x))))
    {
        directional = max(angle, 0.0) * 1.2;
    }
    if (!occluded)
    {
        ao = 0.7;
    }
    vec4 color = texture(atlas, uv);
    color.a = clamp(color.a + alpha, 0.0, 1.0);
    const float light = ao + ambient + directional;
    const float dy = position.y - player_position.y;
    const float dx = distance(position.xz, player_position.xz);
    const float pitch = atan(dy, dx);
    const vec4 sky = vec4(get_sky(pitch), 1.0);
    const vec4 composite = vec4(color.xyz * light, color.a);
    return mix(composite, sky, fog);
}

#endif