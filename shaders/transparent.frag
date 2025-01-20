#version 450

#include "helpers.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in flat vec3 i_normal;
layout(location = 3) in vec4 i_shadow_position;
layout(location = 4) in flat uint i_shadowed;
layout(location = 5) in float i_fog;
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
layout(set = 3, binding = 2) uniform t_view
{
    mat4 u_view;
};
layout(set = 3, binding = 3) uniform t_proj
{
    mat4 u_proj;
};

void main()
{
    vec4 color = texture(s_atlas, i_uv);
    vec4 position = u_view * vec4(i_position, 1.0);
    vec4 uv = u_proj * position;
    uv.xyz /= uv.w;
    uv.xy = uv.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    const float depth = texture(s_position, uv.xy).w;
    const float alpha = clamp(abs(depth - position.z) / 5.0, 0.0, 1.0);
    color = mix(color, vec4(0.2, 0.4, 0.6, 1.0), alpha);
    o_color = get_color(
        color,
        s_shadowmap,
        i_position,
        i_uv,
        i_normal,
        u_player_position,
        i_shadow_position.xyz / i_shadow_position.w,
        u_shadow_vector,
        bool(i_shadowed),
        i_fog,
        1.0);
}