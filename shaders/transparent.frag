#include "light.hlsl"
#include "voxel.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
StructuredBuffer<Light> lightBuffer : register(t1, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0);
};

struct Input
{
    float4 WorldPosition : TEXCOORD0;
    nointerpolation float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
    nointerpolation uint Voxel : TEXCOORD3;
};

static const float kEpsilon = 0.001f;

float4 main(Input input) : SV_Target0
{
    float4 color = atlasTexture.Sample(atlasSampler, input.Texcoord);
    return color;
}
