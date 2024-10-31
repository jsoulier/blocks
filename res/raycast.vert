#version 450

#include "config.glsl"

layout(location = 0) in vec3 position;
layout(location = 0) out vec3 coord;
layout(set = 1, binding = 0) uniform mvp_t
{
    mat4 matrix;
}
mvp;
layout(set = 1, binding = 1) uniform hot_t
{
    ivec3 vector;
}
hit;

void main()
{
    coord = position * raycast_size / 2.0 + vec3(0.5, 0.5, 0.5);
    gl_Position = mvp.matrix * vec4(hit.vector + coord, 1.0);
}