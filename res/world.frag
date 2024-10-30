#version 450

#include "config.glsl"

layout(location = 0) in vec2 uv;
layout(location = 1) in float fog;
layout(location = 0) out vec4 color;
layout(set = 2, binding = 0) uniform sampler2D atlas;

void main()
{
    color = texture(atlas, uv);
    color = mix(color, vec4(sky_bottom_color, 1.0), fog);
}