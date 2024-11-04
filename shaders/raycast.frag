#version 450

#include "config.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec4 o_color;

void main()
{
    o_color = vec4(normalize(i_position), raycast_alpha);
}