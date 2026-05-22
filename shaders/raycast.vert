#include "shader.hlsl"

cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Transform : packoffset(c0);
};

cbuffer UniformBuffer : register(b1, space1)
{
    int3 BlockPosition;
};

struct Output
{
    float4 Position : SV_POSITION;
};

static const float kZ = 0.1f;

Output main(uint vertexID : SV_VertexID)
{
    Output output;
    float3 position = GetCubePosition(vertexID);
    position += BlockPosition + 0.5f;
    output.Position = mul(Transform, float4(position, 1.0f));
    output.Position.z = kZ;
    return output;
}
