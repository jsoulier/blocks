#version 450

#include "config.glsl"

layout(location = 0) in uint i_voxel;
layout(set = 1, binding = 0) uniform position_t
{
    ivec3 vector;
}
u_position;
layout(set = 1, binding = 1) uniform mvp_t
{
    mat4 matrix;
}
u_mvp;

void main()
{
    const uint x = i_voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK;
    const uint y = i_voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK;
    const uint z = i_voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK;
    gl_Position = u_mvp.matrix * vec4(u_position.vector + ivec3(x, y, z), 1.0);
}