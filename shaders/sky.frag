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

Output main(Input input)
{
    Output output;
    output.Color = float4(GetSkyColor(input.LocalPosition), 1.0f);
    output.Position = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return output;
}
