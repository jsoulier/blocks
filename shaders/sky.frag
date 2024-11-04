#version 450

#include "config.glsl"

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec4 o_color;

void main()
{
    const float height = max(i_position.y, 0.0);
    const vec3 sky = mix(sky_bottom_color, sky_top_color, height);
    o_color = vec4(sky, 1.0);
}