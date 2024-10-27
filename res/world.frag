#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 2, binding = 0) uniform sampler2D atlas;

void main()
{
    color = texture(atlas, uv);

    /* debugging */
    color.r -= uv.x * 0.5;
    color.g -= uv.y * 0.5;
}