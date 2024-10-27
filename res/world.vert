#version 450

#define X_BITS 5
#define Y_BITS 5
#define Z_BITS 5
#define U_BITS 4
#define V_BITS 4
#define DIRECTION_BITS 3

#define X_OFFSET (0)
#define Y_OFFSET (X_OFFSET + X_BITS)
#define Z_OFFSET (Y_OFFSET + Y_BITS)
#define U_OFFSET (Z_OFFSET + Z_BITS)
#define V_OFFSET (U_OFFSET + U_BITS)
#define DIRECTION_OFFSET (V_OFFSET + V_BITS)

#define X_MASK ((1 << X_BITS) - 1)
#define Y_MASK ((1 << Y_BITS) - 1)
#define Z_MASK ((1 << Z_BITS) - 1)
#define U_MASK ((1 << U_BITS) - 1)
#define V_MASK ((1 << V_BITS) - 1)
#define DIRECTION_MASK ((1 << DIRECTION_BITS) - 1)

layout(location = 0) in uint voxel;
layout(location = 0) out vec2 uv;
layout(set = 1, binding = 0) uniform mvp_t
{
    mat4 matrix;
} mvp;
layout(set = 1, binding = 1) uniform chunk_t
{
    ivec3 vector;
} chunk;
layout(set = 1, binding = 2) uniform scale_t
{
    vec2 vector;
} scale;

void main()
{
    uint x = voxel >> X_OFFSET & X_MASK;
    uint y = voxel >> Y_OFFSET & Y_MASK;
    uint z = voxel >> Z_OFFSET & Z_MASK;
    uint u = voxel >> U_OFFSET & U_MASK;
    uint v = voxel >> V_OFFSET & V_MASK;
    ivec3 position = chunk.vector + ivec3(x, y, z);
    uv = scale.vector * vec2(u, v);
    gl_Position = mvp.matrix * vec4(position, 1.0);
}