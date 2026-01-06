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
    float3 WorldPosition : TEXCOORD0;
    nointerpolation float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
};

Output main(Input input)
{
    Output output;
    output.WorldPosition = VoxelGetPosition(input.Voxel) + ChunkPosition;
    output.Normal = VoxelGetNormal(input.Voxel);
    output.Position = mul(Transform, float4(output.WorldPosition, 1.0f));
    output.Texcoord = VoxelGetTexcoord(input.Voxel);
    return output;
}