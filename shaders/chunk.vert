#include "voxel.hlsl"

cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Transform : packoffset(c0);
};

cbuffer UniformBuffer : register(b1, space1)
{
    float3 ChunkPosition;
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
    float3 position = VoxelGetPosition(input.Voxel) + ChunkPosition;
    output.Position = mul(Transform, float4(position, 1.0f));
    output.Texcoord = VoxelGetTexcoord(input.Voxel);
    return output;
}