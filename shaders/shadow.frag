#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
StructuredBuffer<Block> blockBuffer : register(t1, space2);

struct Input
{
    float2 Texcoord : TEXCOORD0;
    nointerpolation uint Voxel : TEXCOORD1;
};

void main(Input input)
{
    Block block = blockBuffer[GetBlock(input.Voxel)];
    float3 texcoord = float3(input.Texcoord, GetIndex(input.Voxel, block));
    if (atlasTexture.Sample(atlasSampler, texcoord).a < kEpsilon)
    {
        discard;
    }
}
