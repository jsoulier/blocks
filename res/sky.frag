#version 450

layout(location = 0) in vec3 position;
layout(location = 0) out vec4 color;

void main()
{
    const float y = (position.y + 1.0) / 2.0;
    const vec3 top = vec3(0.2, 0.4, 0.8);
    const vec3 bottom = vec3(0.8, 0.9, 1.0);
    color = vec4(mix(top, bottom, y), 1.0);
}