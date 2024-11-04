#version 450

#include "config.glsl"

layout(location = 0) in uint i_voxel;
layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec4 o_shadow;
layout(location = 3) out float o_fog;
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
layout(set = 1, binding = 2) uniform camera_t
{
    vec3 vector;
}
u_camera;
layout(set = 1, binding = 3) uniform shadow_t
{
    mat4 matrix;
}
u_shadow;

const vec3 normals[6] = vec3[6]
(
    vec3( 0, 0, 1),
    vec3( 0, 0,-1),
    vec3( 1, 0, 0),
    vec3(-1, 0, 0),
    vec3( 0, 1, 0),
    vec3( 0,-1, 0)
);

const mat4 bias = mat4
(
    0.5, 0.0, 0.0, 0.0,
    0.0,-0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

void main()
{
    uint x = i_voxel >> VOXEL_X_OFFSET & VOXEL_X_MASK;
    uint y = i_voxel >> VOXEL_Y_OFFSET & VOXEL_Y_MASK;
    uint z = i_voxel >> VOXEL_Z_OFFSET & VOXEL_Z_MASK;
    uint u = i_voxel >> VOXEL_U_OFFSET & VOXEL_U_MASK;
    uint v = i_voxel >> VOXEL_V_OFFSET & VOXEL_V_MASK;
    uint direction = i_voxel >> VOXEL_DIRECTION_OFFSET & VOXEL_DIRECTION_MASK;
    ivec3 position = u_position.vector + ivec3(x, y, z);
    o_uv.x = u / ATLAS_WIDTH * ATLAS_FACE_WIDTH;
    o_uv.y = v / ATLAS_HEIGHT * ATLAS_FACE_HEIGHT;
    gl_Position = u_mvp.matrix * vec4(position, 1.0);
    o_fog = abs(length(position.xz - u_camera.vector.xz));
    o_fog = clamp(o_fog / world_fog_distance, 0.0, 1.0);
    o_fog = pow(o_fog, world_fog_factor);
    o_normal = normals[direction];
    o_shadow = bias * u_shadow.matrix * vec4(position, 1.0);
}