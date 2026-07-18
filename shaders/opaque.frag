#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
Texture2D<float> shadowTexture : register(t1, space2);
SamplerComparisonState shadowSampler : register(s1, space2);
StructuredBuffer<Block> blockBuffer : register(t2, space2);
StructuredBuffer<Light> lightBuffer : register(t3, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0);
};

cbuffer UniformBuffer : register(b1, space3)
{
    float4x4 ShadowTransform : packoffset(c0);
};

cbuffer UniformBuffer : register(b2, space3)
{
    float3 PlayerPosition : packoffset(c0);
};

cbuffer UniformBuffer : register(b3, space3)
{
    float4 Sun : packoffset(c0);
    float4 SkyTop : packoffset(c1);
    float4 SkyHorizon : packoffset(c2);
    float4 Ambient : packoffset(c3);
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
};

Output main(Input input)
{
    Output output;
    Block block = blockBuffer[GetBlock(input.Voxel)];
    float3 texcoord = float3(input.Texcoord, GetIndex(input.Voxel, block));
    float4 color = atlasTexture.Sample(atlasSampler, texcoord);
    output.Position = input.WorldPosition;
    if (color.a < kEpsilon)
    {
        discard;
        return output;
    }
    float3 albedo = color.rgb;
    float3 normal = GetNormal(input.Voxel, block);
    float3 diffuse = GetDiffuseLight(lightBuffer, LightCount, input.WorldPosition, normal);
    float3 ambient = Ambient.xyz;
    float sun = GetSunLight(shadowTexture, shadowSampler, ShadowTransform, Sun.xyz, Sun.w, input.WorldPosition.xyz, normal, block);
    float3 finalColor;
    if (block.IsFullbright)
    {
        finalColor = albedo;
    }
    else
    {
        finalColor = albedo * (diffuse + ambient * input.AO + sun);
    }
    float3 sky = GetSkyColor(input.WorldPosition.xyz - PlayerPosition, SkyTop.xyz, SkyHorizon.xyz);
    float fog = GetFog(distance(input.WorldPosition.xz, PlayerPosition.xz));
    output.Color = float4(lerp(finalColor, sky, fog), 1.0f);
    return output;
}
