Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    uint Block;
};

struct Input
{
    float2 Texcoord : TEXCOORD0;
    nointerpolation uint IsBlock : TEXCOORD1;
};

static const float kEpsilon = 0.001f;

float4 main(Input input) : SV_Target0
{
    if (!input.IsBlock)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    float4 color = atlasTexture.Sample(atlasSampler, float3(input.Texcoord, Block));
    if (color.a <= kEpsilon)
    {
        discard;
    }
    return color;
}
