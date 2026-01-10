#include "sky.hlsl"

struct Input
{
    float3 LocalPosition : TEXCOORD0;
};

struct Output
{
    float4 Color : SV_Target0;
    float4 Position : SV_Target1;
    float4 Light : SV_Target2;
    uint Voxel : SV_Target3;
};

static const float3 kTop = float3(1.0f, 0.0f, 0.0f);     // bright red
static const float3 kBottom = float3(1.0f, 1.0f, 0.0f);  // bright yellow


Output main(Input input)
{
    Output output;
    output.Color = float4(SkyGetColor(input.LocalPosition, kBottom, kTop), 1.0f);
    output.Voxel = 0;
    return output;
}
