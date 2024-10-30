#version 450

#define ALPHA 0.25

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 color;

void main()
{
    const float value = (position.x + position.y + position.z) / 3.0f;
    color = vec4(value, value, value, ALPHA);
}