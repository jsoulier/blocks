#include "shader.hlsl"

cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Proj : packoffset(c0);
};

cbuffer UniformBuffer : register(b1, space1)
{
    float4x4 View : packoffset(c0);
};

struct Output
{
    float4 Position : SV_Position;
    float3 LocalPosition : TEXCOORD0;
};

Output main(uint vertexID : SV_VertexID)
{
    float4x4 view = View;
    view[0][3] = 0.0f;
    view[1][3] = 0.0f;
    view[2][3] = 0.0f;
    Output output;
    output.LocalPosition = GetCubePosition(vertexID);
    output.Position = mul(Proj, mul(view, float4(output.LocalPosition, 1.0f)));
    return output;
}
