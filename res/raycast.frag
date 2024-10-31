#version 450

#include "config.glsl"

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 color;

void main()
{
    color = vec4(normalize(position), raycast_alpha);
}