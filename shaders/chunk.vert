#include "voxel.hlsl"

cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Transform : packoffset(c0);
};

struct Input
{
    uint Voxel : TEXCOORD0;
};

struct Output
{
    float4 Position : SV_Position;
    float3 Texcoord : TEXCOORD0;
};

Output main(Input input)
{
    Output output;
    output.Position = mul(Transform, float4(VoxelGetPosition(input.Voxel), 1.0f));
    output.Texcoord = VoxelGetTexcoord(input.Voxel);
    return output;
}