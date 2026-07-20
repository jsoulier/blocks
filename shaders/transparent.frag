#include "shader.hlsl"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
Texture2D<float4> positionTexture : register(t1, space2);
SamplerState positionSampler : register(s1, space2);
StructuredBuffer<Material> materialBuffer : register(t2, space2);
StructuredBuffer<Light> lightBuffer : register(t3, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0.x);
};

cbuffer UniformBuffer : register(b1, space3)
{
    float3 PlayerPosition : packoffset(c0);
};

cbuffer UniformBuffer : register(b2, space3)
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

float4 main(Input input) : SV_Target0
{
    uint block = GetBlock(input.Voxel);
    Material material = materialBuffer[block];
    uint index = GetAtlasIndex(input.Voxel, material);
    float3 texcoord = float3(input.Texcoord, index);
    float4 color = atlasTexture.Sample(atlasSampler, texcoord);
    float4 position = input.WorldPosition;
    float3 albedo = color.rgb;
    float3 normal = GetNormal(input.Voxel);
    float3 light = GetLight(lightBuffer, LightCount, position.xyz, normal, material);
    float3 ambient = Ambient.xyz;
    float sunlight = GetSunlight(Sun.xyz, Sun.w, normal, block);
    float3 sky = GetSky(input.WorldPosition.xyz - PlayerPosition, SkyTop.xyz, SkyHorizon.xyz);
    float fog = GetFog(distance(position.xz, PlayerPosition.xz));
    float alpha = color.a;
    if (block == kBlockWater)
    {
        float3 seabed = positionTexture.Sample(positionSampler, input.Fragment).xyz;
        alpha += (input.WorldPosition.y - seabed.y) / 10.0f;
    }
    return float4(lerp(albedo * (light + ambient + sunlight), sky, fog), alpha);
}
