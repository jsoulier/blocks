#ifndef LIGHT_HLSL
#define LIGHT_HLSL

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

float3 LightGet(StructuredBuffer<Light> lights, uint lightCount, float4 position, float3 normal)
{
    static const float kSkipNormalDistance = 0.2f;

    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < lightCount; i++)
    {
        // TODO: performance with branches
        Light light = lights[i];
        float radius = (light.Color & 0xFF000000) >> 24;
        float3 lightPosition = float3(light.X, light.Y, light.Z) + 0.5f;
        float3 offset = lightPosition - position.xyz;
        float distance = length(offset);
        if (distance >= radius || radius <= 0.0f)
        {
            continue;
        }
        float3 lightDirection = offset / distance;
        float NdotL;
        if (distance > kSkipNormalDistance)
        {
            NdotL = saturate(dot(normal, lightDirection));
            if (distance > kSkipNormalDistance && NdotL <= 0.0f)
            {
                continue;
            }
        }
        else
        {
            NdotL = 1.0f;
        }
        float attenuation = 1.0f - (distance / radius);
        attenuation = saturate(attenuation);
        attenuation *= attenuation;
        float3 color;
        color.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        color.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        color.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        finalColor += color * NdotL * attenuation;
    }
    return finalColor;
}

#endif