#ifndef RANDOM_HLSL
#define RANDOM_HLSL

float2 Random2(float2 position)
{
    float2 k1 = float2(127.1f, 311.7f);
    float2 k2 = float2(269.5f, 183.3f);
    float n = sin(dot(position, k1)) * 43758.5453f;
    float m = sin(dot(position, k2)) * 43758.5453f;
    float2 r = frac(float2(n, m));
    return r * 2.0f - 1.0f;
}

float2 Random2(float3 position)
{
    float3 k1 = float3(127.1, 311.7, 74.7);
    float3 k2 = float3(269.5, 183.3, 246.1);
    float n = sin(dot(position, k1)) * 43758.5453f;
    float m = sin(dot(position, k2)) * 43758.5453f;
    float2 r = frac(float2(n, m));
    return r * 2.0f - 1.0f;
}

#endif
