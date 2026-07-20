#include "shader.hlsl"

struct Input
{
    float3 LocalPosition : TEXCOORD0;
};

struct Output
{
    float4 Color : SV_Target0;
    float4 Position : SV_Target1;
};

cbuffer UniformBuffer : register(b0, space3)
{
    float4 Sun : packoffset(c0);
    float4 SkyTop : packoffset(c1);
    float4 SkyHorizon : packoffset(c2);
    float4 Ambient : packoffset(c3);
};

Output main(Input input)
{
    Output output;
    output.Color = float4(GetSky(input.LocalPosition, SkyTop.xyz, SkyHorizon.xyz), 1.0f);
    output.Position = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return output;
}
