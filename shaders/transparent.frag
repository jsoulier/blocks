#include "light.hlsl"
#include "shadow.hlsl"
#include "voxel.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
Texture2D<float> shadowTexture : register(t1, space2);
SamplerState shadowSampler : register(s1, space2);
StructuredBuffer<Light> lightBuffer : register(t2, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0.x);
};

cbuffer UniformBuffer : register(b1, space3)
{
    float4x4 ShadowTransform : packoffset(c0);
};

struct Input
{
    float4 WorldPosition : TEXCOORD0;
    nointerpolation float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
    nointerpolation uint Voxel : TEXCOORD3;
};

// TODO: sky
static const float3 kAmbient = float3(0.5f, 0.5f, 0.5f);

float4 main(Input input) : SV_Target0
{
    float4 color = atlasTexture.Sample(atlasSampler, input.Texcoord);
    float3 albedo = color.rgb;
    float alpha = color.a;
    float4 position = input.WorldPosition;
    float3 diffuse = LightGet(lightBuffer, LightCount, position, input.Normal);
    float shadow = ShadowGet(shadowTexture, shadowSampler, ShadowTransform, position.xyz, input.Normal);
    float3 finalColor = albedo * (diffuse + kAmbient - shadow);
    return float4(finalColor, alpha);
}
