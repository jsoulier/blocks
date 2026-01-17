#include "shader.hlsl"

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

Output main(Input input)
{
    Output output;
    output.Color = float4(GetSkyColor(input.LocalPosition), 1.0f);
    output.Voxel = 0;
    return output;
}
