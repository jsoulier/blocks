#version 450

#include "helpers.glsl"

layout(location = 0) in uint i_voxel;
layout(location = 0) out flat uint o_voxel;
layout(location = 1) out vec3 o_position;
layout(location = 2) out vec2 o_uv;
layout(set = 1, binding = 0) uniform t_position
{
    ivec3 u_position;
};
layout(set = 1, binding = 1) uniform t_view
{
    mat4 u_view;
};
layout(set = 1, binding = 2) uniform t_proj
{
    mat4 u_proj;
};

void main()
{
    o_voxel = i_voxel;
    o_position = u_position + get_position(i_voxel);
    o_uv = get_uv(i_voxel);
    vec4 position = u_view * vec4(o_position, 1.0);
    gl_Position = u_proj * position;
}