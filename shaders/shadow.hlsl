#ifndef SHADOW_HLSL
#define SHADOW_HLSL

float ShadowGet(Texture2D<float> texture, SamplerState sampler, float4x4 transform, float3 position, float3 normal)
{
    static const float kBias = 0.00015f;
    static const float kShadow = 0.4f;
    float4 shadowPosition = mul(transform, float4(position, 1.0f));
    shadowPosition.xyz /= shadowPosition.w;
    float2 uv = shadowPosition.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        return 0.0f;
    }
    float3 direction = normalize(float3(transform[2].xyz));
    float depth = shadowPosition.z;
    float closestDepth = texture.SampleLevel(sampler, uv, 0);
    float ratio = dot(normal, direction);
    if (ratio > 0.0f)
    {
        return kShadow;
    }
    if (depth - kBias <= closestDepth)
    {
        return 0.0f;
    }
    else
    {
        return kShadow;
    }
}

#endif
