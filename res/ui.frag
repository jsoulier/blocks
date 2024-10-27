#version 450

layout(location = 0) in vec2 position;
layout(location = 0) out vec4 color;

void main()
{
    color = vec4(position, 0.0f, 1.0f);
}