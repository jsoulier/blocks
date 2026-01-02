Texture2DArray<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct Input
{
    float3 Texcoord : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return Texture.Sample(Sampler, input.Texcoord);
}