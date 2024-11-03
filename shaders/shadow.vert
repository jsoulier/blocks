#version 450

#include "config.glsl"

layout(location = 0) in uint voxel;
layout(set = 1, binding = 0) uniform chunk_t
{
    ivec3 vector;
}
chunk;
layout(set = 1, binding = 1) uniform view_t
{
    mat4 matrix;
}
view;
layout(set = 1, binding = 2) uniform proj_t
{
    mat4 matrix;
}
proj;

void main()
{
    uint x = voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK;
    uint y = voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK;
    uint z = voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK;
    ivec3 position = chunk.vector + ivec3(x, y, z);
    mat4 matrix = view.matrix;
    gl_Position = proj.matrix * matrix * vec4(position, 1.0);
}