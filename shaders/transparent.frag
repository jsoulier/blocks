#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
Texture2D<float> shadowTexture : register(t1, space2);
SamplerState shadowSampler : register(s1, space2);
Texture2D<float4> positionTexture : register(t2, space2);
SamplerState positionSampler : register(s2, space2);
StructuredBuffer<Block> blockBuffer : register(t3, space2);
StructuredBuffer<Light> lightBuffer : register(t4, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0.x);
};

cbuffer UniformBuffer : register(b1, space3)
{
    float4x4 ShadowTransform : packoffset(c0);
};

cbuffer UniformBuffer : register(b2, space3)
{
    float3 PlayerPosition : packoffset(c0);
};

struct Input
{
    float4 WorldPosition : TEXCOORD0;
    float2 Texcoord : TEXCOORD1;
    nointerpolation uint Voxel : TEXCOORD2;
    float2 Fragment : TEXCOORD3;
};

static const uint kWater = 16;

float4 main(Input input) : SV_Target0
{
    Block block = blockBuffer[GetBlock(input.Voxel)];
    float3 normal = GetNormal(input.Voxel, block);
    float3 texcoord = float3(input.Texcoord, GetIndex(input.Voxel, block));
    float4 color = atlasTexture.Sample(atlasSampler, texcoord);
    float3 albedo = color.rgb;
    float alpha = color.a;
    float4 position = input.WorldPosition;
    float3 diffuse = GetDiffuseLight(lightBuffer, LightCount, position, normal);
    float3 ambient = GetAmbientLight();
    float sun = GetSunLight(shadowTexture, shadowSampler, ShadowTransform, position.xyz, normal, block);
    float3 sky = GetSkyColor(input.WorldPosition.xyz - PlayerPosition);
    float fog = GetFog(distance(position.xz, PlayerPosition.xz));
    if (GetIndex(input.Voxel, block) == kWater)
    {
        // TODO: Causes bug where alpha is 0 or 1 as camera approaches water
        float3 groundPosition = positionTexture.Sample(positionSampler, input.Fragment).xyz;
        alpha += (input.WorldPosition.y - groundPosition.y) / 10.0f;
    }
    return float4(lerp(albedo * (diffuse + ambient + sun), sky, fog), alpha);
}
