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
    float ClipDistance : SV_ClipDistance0;
};

Output main(Input input)
{
    Output output;
    int3 chunkPosition = float3(ChunkPosition.x, 0.0f, ChunkPosition.y);
    float3 position = GetPosition(input.Voxel) + chunkPosition;
    output.Position = mul(Proj, mul(View, float4(position, 1.0f)));
    output.ClipDistance = GetShadow(input.Voxel) ? 1.0f : -1.0f;
    return output;
}
