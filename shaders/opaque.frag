#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
StructuredBuffer<Block> blockBuffer : register(t1, space2);
StructuredBuffer<Light> lightBuffer : register(t2, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0);
};

struct Input
{
    float4 WorldPosition : TEXCOORD0;
    float2 Texcoord : TEXCOORD1;
    nointerpolation uint Voxel : TEXCOORD2;
    float AO : TEXCOORD3;
};

struct Output
{
    float4 Color : SV_Target0;
    float4 Position : SV_Target1;
    float4 Light : SV_Target2;
    uint Voxel : SV_Target3;
};

Output main(Input input)
{
    Output output;
    Block block = blockBuffer[GetBlock(input.Voxel)];
    float3 normal = GetNormal(input.Voxel, block);
    float3 texcoord = float3(input.Texcoord, GetIndex(input.Voxel, block));
    output.Color = atlasTexture.Sample(atlasSampler, texcoord);
    output.Position = input.WorldPosition;
    output.Light = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Voxel = 0;
    if (output.Color.a < kEpsilon)
    {
        discard;
        return output;
    }
    output.Color.a = input.AO;
    output.Voxel = input.Voxel & MATERIAL_MASK;
    output.Light.rgb = GetDiffuseLight(lightBuffer, LightCount, input.WorldPosition, normal);
    return output;
}
