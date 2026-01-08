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
    float3 Normal : TEXCOORD1;
    float3 Texcoord : TEXCOORD2;
};

static const float kLightIntensity = 10.0f;
static const float kEpsilon = 0.001f;
static const float3 kAmbient = float3(0.2f, 0.2f, 0.2f);

float4 main(Input input) : SV_Target0
{
    float4 albedo = atlasTexture.Sample(atlasSampler, input.Texcoord);
    // TODO: handle for transparents
    if (albedo.a < kEpsilon)
    {
        discard;
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    float3 diffuse = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < LightCount; i++)
    {
        Light light = lightBuffer[i];
        float3 color;
        color.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        color.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        color.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        float intensity = ((light.Color & 0xFF000000) >> 24) / 255.0f;
        intensity *= kLightIntensity;
        float3 lightPosition = float3(light.X, light.Y, light.Z) + 0.5f;
        float3 offset = lightPosition - input.WorldPosition;
        float dist2 = max(dot(offset, offset), kEpsilon);
        float3 L = offset * rsqrt(dist2);
        float NdotL = max(dot(input.Normal, L), 0.0f);
        float attenuation = 1.0f / dist2;
        diffuse += color * NdotL * attenuation * intensity;
    }
    float3 color = albedo.rgb * (diffuse + kAmbient);
    return float4(color, 1.0);
}
