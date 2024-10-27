#version 450

layout(location = 0) in vec2 position;
layout(location = 0) out vec2 ndc;

void main()
{
    gl_Position = vec4(position, 0.0f, 1.0f);
    ndc = position;
}