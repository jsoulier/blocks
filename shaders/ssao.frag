#version 450

#include "helpers.glsl"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out float o_ssao;
layout(set = 2, binding = 0) uniform sampler2D s_position;
layout(set = 2, binding = 1) uniform sampler2D s_uv;
layout(set = 2, binding = 2) uniform usampler2D s_voxel;
layout(set = 2, binding = 3) uniform sampler2D s_random;

bool test(
    const uint direction,
    const vec3 position,
    const vec2 uv)
{
    const vec3 neighbor_position = texture(s_position, uv).xyz;
    const vec2 neighbor_uv = texture(s_uv, uv).xy;
    if (length(neighbor_uv) == 0.0)
    {
        return false;
    }
    const float bias = 0.01;
    float values[6];
    values[0] = neighbor_position.z - position.z;
    values[1] = position.z - neighbor_position.z;
    values[2] = neighbor_position.x - position.x;
    values[3] = position.x - neighbor_position.x;
    values[4] = neighbor_position.y - position.y;
    values[5] = position.y - neighbor_position.y;
    return values[direction] > bias;
}

void main()
{
    const vec2 uv = texture(s_uv, i_uv).xy;
    const uint voxel = texture(s_voxel, i_uv).x;
    if (!get_occluded(voxel) || length(uv) == 0)
    {
        discard;
    }
    const vec4 position = texture(s_position, i_uv);
    const uint direction = get_direction(voxel);
    const vec2 scale = 75.0 / (textureSize(s_voxel, 0) * position.w);
    float ssao = 0.0;
    int kernel = 2;
    for (int x = -kernel; x <= kernel; x++)
    {
        for (int y = -kernel; y <= kernel; y++)
        {
            const vec2 origin = i_uv + vec2(x, y) * scale;
            const vec2 offset = texture(s_random, origin).xy * scale;
            ssao += float(test(direction, position.xyz, origin + offset));
        }
    }
    kernel = kernel * 2 + 1;
    kernel = kernel * kernel;
    kernel -= 1;
    o_ssao = 1.0 - (ssao / float(kernel));
}