#version 450

layout(location = 0) in vec3 i_position;
layout(location = 0) out vec3 o_position;
layout(set = 1, binding = 0) uniform view_t
{
    mat4 matrix;
}
u_view;
layout(set = 1, binding = 1) uniform proj_t
{
    mat4 matrix; 
}
u_proj;

void main()
{
    mat4 rotation = u_view.matrix;
    rotation[3][0] = 0.0;
    rotation[3][1] = 0.0;
    rotation[3][2] = 0.0;
    gl_Position = u_proj.matrix  * rotation * vec4(i_position, 1.0);
    o_position = i_position;
}