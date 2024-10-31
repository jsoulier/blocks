#version 450

#include "config.glsl"

layout(location = 0) in uint voxel;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 normal;
layout(location = 2) out float fog;
layout(set = 1, binding = 0) uniform mvp_t
{
    mat4 matrix;
}
mvp;
layout(set = 1, binding = 1) uniform eye_t
{
    vec3 position;
}
eye;
layout(set = 1, binding = 2) uniform chunk_t
{
    ivec3 vector;
}
chunk;

const vec3 normals[6] = vec3[6]
(
    vec3( 0, 0, 1),
    vec3( 0, 0,-1),
    vec3( 1, 0, 0),
    vec3(-1, 0, 0),
    vec3( 0, 1, 0),
    vec3( 0,-1, 0)
);
void main()
{
    uint x = voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK;
    uint y = voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK;
    uint z = voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK;
    uint u = voxel >> VOXEL_U_OFFSET & VOXEL_U_MASK;
    uint v = voxel >> VOXEL_V_OFFSET & VOXEL_V_MASK;
    uint direction = voxel >> VOXEL_DIRECTION_OFFSET & VOXEL_DIRECTION_MASK;
    ivec3 position = chunk.vector + ivec3(x, y, z);
    uv.x = u / ATLAS_WIDTH * ATLAS_FACE_WIDTH;
    uv.y = v / ATLAS_HEIGHT * ATLAS_FACE_HEIGHT;
    gl_Position = mvp.matrix * vec4(position, 1.0);
    fog = abs(length(position.xz - eye.position.xz));
    fog = clamp(fog / world_fog_distance, 0.0, 1.0);
    fog = pow(fog, world_fog_factor);
    normal = normals[direction];
}