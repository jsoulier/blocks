#version 450

#include "helpers.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in flat vec3 i_normal;
layout(location = 3) in vec4 i_shadow_position;
layout(location = 4) in flat uint i_shadowed;
layout(location = 5) in float i_fog;
layout(location = 6) in vec2 i_fragment;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D s_atlas;
layout(set = 2, binding = 1) uniform sampler2D s_shadowmap;
layout(set = 2, binding = 2) uniform sampler2D s_position;
layout(set = 3, binding = 0) uniform t_shadow_vector
{
    vec3 u_shadow_vector;
};
layout(set = 3, binding = 1) uniform t_player_position
{
    vec3 u_player_position;
};

void main()
{
    o_color = get_color(
        s_atlas,
        s_shadowmap,
        i_position,
        i_uv,
        i_normal,
        u_player_position,
        i_shadow_position.xyz / i_shadow_position.w,
        u_shadow_vector,
        bool(i_shadowed),
        i_fog,
        1.0,
        (i_position.y - texture(s_position, i_fragment).y) / 20.0);
}