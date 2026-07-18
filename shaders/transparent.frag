#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
Texture2D<float> shadowTexture : register(t1, space2);
SamplerComparisonState shadowSampler : register(s1, space2);
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
    noperspective float2 Fragment : TEXCOORD3;
};

static const uint kWater = 16;

float4 main(Input input) : SV_Target0
{
    Block block = blockBuffer[GetBlockIndex(input.Voxel)];
    float3 normal = GetNormal(input.Voxel);
    uint index = GetAtlasIndex(input.Voxel, block);
    float3 texcoord = float3(input.Texcoord, index);
    float4 texel = atlasTexture.Sample(atlasSampler, texcoord);
    float3 albedo = texel.rgb;
    float alpha = texel.a;
    float4 position = input.WorldPosition;
    float3 pointLight = GetPointLight(lightBuffer, LightCount, position.xyz, normal);
    float3 ambient = Ambient.xyz;
    float sunlight =
        GetSunlight(shadowTexture, shadowSampler, ShadowTransform, Sun.xyz, Sun.w, position.xyz, normal, block);
    float3 sky = GetSkyColor(input.WorldPosition.xyz - PlayerPosition, SkyTop.xyz, SkyHorizon.xyz);
    float fog = GetFogValue(distance(position.xz, PlayerPosition.xz));
    if (index == kWater)
    {
        float3 groundPosition = positionTexture.Sample(positionSampler, input.Fragment).xyz;
        alpha += (input.WorldPosition.y - groundPosition.y) / 10.0f;
    }
    float3 litColor = albedo * (pointLight + ambient + sunlight);
    return float4(lerp(litColor, sky, fog), alpha);
}
