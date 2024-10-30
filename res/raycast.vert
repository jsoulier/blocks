#version 450

layout(location = 0) in vec3 position;
layout(location = 0) out vec3 coord;
layout(set = 1, binding = 0) uniform mvp_t {
    mat4 matrix;
} mvp;
layout(set = 1, binding = 1) uniform raycast_t {
    ivec3 vector;
} raycast;

void main()
{
    coord = position / 2.0 + vec3(0.5, 0.5, 0.5);
    gl_Position = mvp.matrix * vec4(raycast.vector + coord, 1.0);
}