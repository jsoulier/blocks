Texture2DArray<float4> AtlasTexture : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

StructuredBuffer<Light> LightBuffer : register(t1, space2);

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

float4 main(Input input) : SV_Target0
{
    float3 albedo = AtlasTexture.Sample(AtlasSampler, input.Texcoord).rgb;
    float3 N = normalize(input.Normal);
    float3 lighting = float3(0, 0, 0);

    for (uint i = 0; i < LightCount; i++)
    {
        Light light = LightBuffer[i];
        float3 color;
        color.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        color.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        color.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        float intensity = ((light.Color & 0xFF000000) >> 24) / 255.0f;

        intensity *= 10.0f;

        float3 lightPos = float3(light.X, light.Y, light.Z) + 0.5f;
        float3 L = lightPos - input.WorldPosition;

        float distSq = dot(L, L);
        float dist = sqrt(distSq);

        L /= dist;
        float NdotL = saturate(dot(N, L));
        float attenuation = intensity / (1.0 + distSq * 0.02);

        lighting += color * NdotL * attenuation;
    }
    
    float3 ambient = albedo * 0.1;
    float3 finalColor = albedo * lighting + ambient;
    return float4(finalColor, 1.0);
}