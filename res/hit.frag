#version 450

#define ALPHA 0.25

layout(location = 0) out vec4 color;

void main()
{
    color = vec4(1.0, 1.0, 1.0, ALPHA);
}