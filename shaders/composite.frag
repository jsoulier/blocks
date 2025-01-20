#version 450

#include "helpers.glsl"

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_atlas;
layout(set = 2, binding = 1) uniform sampler2D s_position;
layout(set = 2, binding = 2) uniform sampler2D s_uv;
layout(set = 2, binding = 3) uniform usampler2D s_voxel;
layout(set = 2, binding = 4) uniform sampler2D s_shadowmap;
layout(set = 2, binding = 5) uniform sampler2D s_ssao;
layout(set = 3, binding = 0) uniform t_player_position
{
    vec3 u_player_position;
};
layout(set = 3, binding = 1) uniform t_shadow_vector
{
    vec3 u_shadow_vector;
};
layout(set = 3, binding = 2) uniform t_shadow_matrix
{
    mat4 u_shadow_matrix;
};

void main()
{
    const vec3 position = texture(s_position, i_uv).xyz;
    const vec2 uv = texture(s_uv, i_uv).xy;
    const uint voxel = texture(s_voxel, i_uv).x;
    if (length(uv) == 0)
    {
        discard;
    }
    const vec4 shadow_position = u_shadow_matrix * vec4(position, 1.0);
    o_color = get_color(
        s_atlas,
        s_shadowmap,
        position,
        uv,
        get_normal(voxel),
        u_player_position,
        shadow_position.xyz / shadow_position.w,
        u_shadow_vector,
        get_shadowed(voxel),
        get_fog(distance(position.xz, u_player_position.xz)),
        texture(s_ssao, i_uv).r,
        0.0);
}