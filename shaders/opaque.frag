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

struct Output
{
    float4 Color : SV_Target0;
    float4 Position : SV_Target1;
    float4 Light : SV_Target2;
    uint Voxel : SV_Target3;
};

static const float kEpsilon = 0.001f;

Output main(Input input)
{
    Output output;
    output.Color = atlasTexture.Sample(atlasSampler, input.Texcoord);
    output.Position = input.WorldPosition;
    output.Light = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Voxel = 0;
    if (output.Color.a < kEpsilon)
    {
        discard;
        return output;
    }
    output.Voxel |= input.Voxel & (VOXEL_OCCLUSION_MASK << VOXEL_OCCLUSION_OFFSET);
    output.Voxel |= input.Voxel & (VOXEL_DIRECTION_MASK << VOXEL_DIRECTION_OFFSET);
    output.Light.rgb = LightGet(lightBuffer, LightCount, input.WorldPosition, input.Normal);
    return output;
}
