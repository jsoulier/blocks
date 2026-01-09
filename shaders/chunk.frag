#include "voxel.hlsl"

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);
StructuredBuffer<Light> lightBuffer : register(t1, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int LightCount : packoffset(c0);
};

struct Input
{
    float3 WorldPosition : TEXCOORD0;
    nointerpolation float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
    nointerpolation uint Voxel : TEXCOORD3;
};

struct Output
{
    float4 Color : SV_Target0;
    uint Voxel : SV_Target1;
};

static const float kEpsilon = 0.001f;
static const float3 kAmbient = float3(0.2f, 0.2f, 0.2f);

Output main(Input input)
{
    Output output;
    output.Color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Voxel = 0;
    float4 albedo = atlasTexture.Sample(atlasSampler, input.Texcoord);
    // TODO: handle for transparents
    if (albedo.a < kEpsilon)
    {
        discard;
        return output;
    }
    float3 diffuse = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < LightCount; i++)
    {
        // TODO: performance with branches
        Light light = lightBuffer[i];
        float radius = (light.Color & 0xFF000000) >> 24;
        float3 lightPosition = float3(light.X, light.Y, light.Z) + 0.5f;
        float3 offset = lightPosition - input.WorldPosition;
        float distance = length(offset);
        if (distance >= radius || radius <= 0.0f)
        {
            continue;
        }
        float3 lightDirection = offset / distance;
        float NdotL = saturate(dot(input.Normal, lightDirection));
        if (NdotL <= 0.0f)
        {
            continue;
        }
        float attenuation = 1.0f - (distance / radius);
        attenuation = saturate(attenuation);
        attenuation *= attenuation;
        float3 color;
        color.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        color.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        color.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        diffuse += color * NdotL * attenuation;
    }
    float3 color = albedo.rgb * (diffuse + kAmbient);
    output.Color = float4(color, 1.0f);
    // TODO: transparents aren't valid
    output.Voxel |= 1 << VOXEL_VALID_OFFSET;
    output.Voxel |= input.Voxel & (VOXEL_DIRECTION_MASK << VOXEL_DIRECTION_OFFSET);
    return output;
}
