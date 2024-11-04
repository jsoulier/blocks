#version 450

#include "config.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec3 o_position;
layout(set = 1, binding = 0) uniform mvp_t
{
    mat4 matrix;
}
u_mvp;
layout(set = 1, binding = 1) uniform position_t
{
    ivec3 vector;
}
u_position;

void main()
{
    o_position = i_position * raycast_block / 2.0 + vec3(0.5, 0.5, 0.5);
    gl_Position = u_mvp.matrix * vec4(u_position.vector + o_position, 1.0);
}