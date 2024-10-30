#version 450

#include "config.glsl"

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 color;

void main()
{
    const float y = (position.y + 1.0) / 2.0;
    // color = vec4(mix(sky_top_color, sky_bottom_color, y), 1.0);
    color = vec4(sky_bottom_color, 1.0);
}