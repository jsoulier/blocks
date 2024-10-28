#version 450

layout(location = 0) in vec3 position;
layout(set = 1, binding = 0) uniform mvp_t {
    mat4 matrix;
} mvp;
layout(set = 1, binding = 1) uniform hit_t {
    ivec3 vector;
} hit;

void main()
{
    const vec3 cube = position / 2.0 + vec3(0.5, 0.5, 0.5);
    gl_Position = mvp.matrix * vec4(hit.vector + cube, 1.0);
}