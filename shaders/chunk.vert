#include "shader.hlsl"

cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Proj;
};

cbuffer UniformBuffer : register(b1, space1)
{
    float4x4 View;
};

cbuffer UniformBuffer : register(b2, space1)
{
    int2 ChunkPosition;
};

struct Input
{
    uint Voxel : TEXCOORD0;
};

struct Output
{
    float4 Position : SV_Position;
    float4 WorldPosition : TEXCOORD0;
    nointerpolation float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
    nointerpolation uint Voxel : TEXCOORD3;
};

Output main(Input input)
{
    Output output;
    int3 chunkPosition = float3(ChunkPosition.x, 0.0f, ChunkPosition.y);
    output.WorldPosition.xyz = GetVoxelPosition(input.Voxel) + chunkPosition;
    output.Normal = GetVoxelNormal(input.Voxel);
    output.Position = mul(View, float4(output.WorldPosition.xyz, 1.0f));
    output.WorldPosition.w = output.Position.z;
    output.Position = mul(Proj, output.Position);
    output.Texcoord = GetVoxelTexcoord(input.Voxel);
    output.Voxel = input.Voxel;
    return output;
}
