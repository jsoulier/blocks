#ifndef SHADER_HLSL
#define SHADER_HLSL

#include "voxel.inc"

static const float kEpsilon = 0.001f;
static const float kPi = 3.14159265f;

static const float3 kPositions[10] =
{
    float3(-0.50f,-0.50f,-0.50f ),
    float3( 0.50f,-0.50f,-0.50f ),
    float3( 0.50f, 0.50f,-0.50f ),
    float3(-0.50f, 0.50f,-0.50f ),
    float3(-0.50f,-0.50f, 0.50f ),
    float3( 0.50f,-0.50f, 0.50f ),
    float3( 0.50f, 0.50f, 0.50f ),
    float3(-0.50f, 0.50f, 0.50f ),
    float3( 0.00f, 0.00f, 0.00f ),
    float3( 0.00f, 0.00f, 0.00f ),
};

static const float3 kNormals[10] =
{
    float3( 0.00f, 0.00f, 1.00f ),
    float3( 0.00f, 0.00f,-1.00f ),
    float3( 1.00f, 0.00f, 0.00f ),
    float3(-1.00f, 0.00f, 0.00f ),
    float3( 0.00f, 1.00f, 0.00f ),
    float3( 0.00f,-1.00f, 0.00f ),
    float3(-0.77f, 0.00f, 0.77f ),
    float3( 0.77f, 0.00f,-0.77f ),
    float3(-0.77f, 0.00f,-0.77f ),
    float3( 0.77f, 0.00f, 0.77f ),
};

static const uint kIndices[36] =
{
    0, 1, 2, 0, 2, 3,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    1, 5, 6, 1, 6, 2,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0
};

bool GetOcclusion(uint voxel)
{
    return (voxel >> OCCLUSION_OFFSET) & OCCLUSION_MASK;
}

uint GetDirection(uint voxel)
{
    return ((voxel >> DIRECTION_OFFSET) & DIRECTION_MASK) - 1;
}

bool GetShadow(uint voxel)
{
    return (voxel >> SHADOW_OFFSET) & SHADOW_MASK;
}

float3 GetPosition(uint voxel)
{
    return float3((voxel >> X_OFFSET) & X_MASK, (voxel >> Y_OFFSET) & Y_MASK, (voxel >> Z_OFFSET) & Z_MASK);
}

uint GetIndex(uint voxel)
{
    return (voxel >> INDEX_OFFSET) & INDEX_MASK;
}

float3 GetTexcoord(uint voxel)
{
    return float3((voxel >> U_OFFSET) & U_MASK, (voxel >> V_OFFSET) & V_MASK, GetIndex(voxel));
}

float3 GetNormal(uint voxel)
{
    return kNormals[GetDirection(voxel)];
}

float3 GetCubePosition(uint vertexID)
{
    return kPositions[kIndices[vertexID]];
}

bool IsCloud(float3 color)
{
    return length(color) > (1.0f - kEpsilon);
}

bool IsSky(uint voxel)
{
    return voxel == 0;
}

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

float3 GetAmbientLight()
{
    return float3(0.5f, 0.5f, 0.5f);
}

float3 GetDiffuseLight(StructuredBuffer<Light> lights, uint lightCount, float4 position, float3 normal)
{
    static const float kBias = 0.1f;
    static const float kLight = 2.0f;
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < lightCount; i++)
    {
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
        if (distance > kBias)
        {
            NdotL = saturate(dot(normal, lightDirection));
            if (NdotL <= 0.0f)
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
    return finalColor * kLight;
}

float GetSunLight(Texture2D<float> texture, SamplerState sampler, float4x4 transform, float3 position, float3 normal, uint voxel)
{
    static const float kBias = 0.001f;
    static const float kBase = 0.0f;
    static const float kShadow = 0.4f;
    float4 shadowPosition = mul(transform, float4(position, 1.0f));
    shadowPosition.xyz /= shadowPosition.w;
    float2 uv = shadowPosition.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        return kBase + kShadow;
    }
    float3 direction = normalize(float3(transform[2].xyz));
    float ratio = -0.707f;
    if (GetOcclusion(voxel))
    {
        ratio = dot(normal, direction);
        if (ratio > 0.0f)
        {
            return 0.0f;
        }
    }
    float depth = shadowPosition.z;
    float closestDepth = texture.SampleLevel(sampler, uv, 0);
    if (depth < closestDepth + kBias)
    {
        return kBase - kShadow * ratio;
    }
    else
    {
        return 0.0f;
    }
}

float3 GetSkyColor(float3 position)
{
    static const float3 kTop = float3(0.212f, 0.773f, 0.957f);
    static const float3 kBottom = float3(0.220f, 0.349f, 0.702f);
    float dy = position.y;
    float dx = length(float2(position.x, position.z));
    float pitch = atan2(dy, dx);
    float alpha = (pitch + kPi / 2.0f) / kPi;
    return lerp(kBottom, kTop, alpha);
}

float GetFog(const float x)
{
    return min(pow(x / 250.0f, 2.5f), 1.0f);
}

float2 GetRandom2(float2 position)
{
    float2 k1 = float2(127.1f, 311.7f);
    float2 k2 = float2(269.5f, 183.3f);
    float n = sin(dot(position, k1)) * 43758.5453f;
    float m = sin(dot(position, k2)) * 43758.5453f;
    float2 r = frac(float2(n, m));
    return r * 2.0f - 1.0f;
}

float2 GetRandom2(float3 position)
{
    float3 k1 = float3(127.1, 311.7, 74.7);
    float3 k2 = float3(269.5, 183.3, 246.1);
    float n = sin(dot(position, k1)) * 43758.5453f;
    float m = sin(dot(position, k2)) * 43758.5453f;
    float2 r = frac(float2(n, m));
    return r * 2.0f - 1.0f;
}

#endif
