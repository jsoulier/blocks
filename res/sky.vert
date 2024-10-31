#version 450

layout(location = 0) in vec3 position;
layout(location = 0) out vec3 p;
layout(set = 1, binding = 0) uniform view_t
{
    mat4 matrix;
}
view;
layout(set = 1, binding = 1) uniform proj_t
{
    mat4 matrix; 
}
proj;

void main()
{
    mat4 rotation = view.matrix;
    rotation[3][0] = 0.0;
    rotation[3][1] = 0.0;
    rotation[3][2] = 0.0;
    gl_Position = proj.matrix  * rotation * vec4(position, 1.0);
    p = position;
}