#ifndef SHADER_HLSL
#define SHADER_HLSL

#include "voxel.inc"

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

static float3 GetCubeVertex(uint vertexID)
{
    static const float3 kVertices[8] =
    {
        float3(-0.5f,-0.5f,-0.5f),
        float3( 0.5f,-0.5f,-0.5f),
        float3( 0.5f, 0.5f,-0.5f),
        float3(-0.5f, 0.5f,-0.5f),
        float3(-0.5f,-0.5f, 0.5f),
        float3( 0.5f,-0.5f, 0.5f),
        float3( 0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, 0.5f) 
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
    return kVertices[kIndices[vertexID]];
}

float3 GetLight(StructuredBuffer<Light> lights, uint lightCount, float4 position, float3 normal)
{
    static const float kBias = 0.2f;
    static const float kLight = 2.0f;
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

float GetShadow(Texture2D<float> texture, SamplerState sampler, float4x4 transform, float3 position, float3 normal)
{
    static const float kBias = 0.001f;
    static const float kShadow = 0.4f;
    float4 shadowPosition = mul(transform, float4(position, 1.0f));
    // The w component should be 1 since it's an orthographic projection
    // shadowPosition.xyz /= shadowPosition.w;
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

float3 GetSkyColor(float3 position)
{
    static const float3 kTop = float3(1.0f, 0.0f, 0.0f);
    static const float3 kBottom = float3(1.0f, 1.0f, 0.0f);
    static const float kPi = 3.14159265f;
    float dy = position.y;
    float dx = length(float2(position.x, position.z));
    float pitch = atan2(dy, dx);
    float alpha = (pitch + kPi / 2.0f) / kPi;
    return lerp(kBottom, kTop, alpha);
}

bool GetVoxelOcclusion(uint voxel)
{
    return (voxel >> VOXEL_OCCLUSION_OFFSET) & VOXEL_OCCLUSION_MASK;
}

uint GetVoxelDirection(uint voxel)
{
    return (voxel >> VOXEL_DIRECTION_OFFSET) & VOXEL_DIRECTION_MASK;
}

bool GetVoxelShadow(uint voxel)
{
    return (voxel >> VOXEL_SHADOW_OFFSET) & VOXEL_SHADOW_MASK;
}

float3 GetVoxelPosition(uint voxel)
{
    return float3((voxel >> VOXEL_X_OFFSET) & VOXEL_X_MASK, (voxel >> VOXEL_Y_OFFSET) & VOXEL_Y_MASK, (voxel >> VOXEL_Z_OFFSET) & VOXEL_Z_MASK);
}

float3 GetVoxelTexcoord(uint voxel)
{
    return float3((voxel >> VOXEL_U_OFFSET) & VOXEL_U_MASK, (voxel >> VOXEL_V_OFFSET) & VOXEL_V_MASK, (voxel >> VOXEL_INDEX_OFFSET) & VOXEL_INDEX_MASK);
}

float3 GetVoxelNormal(uint voxel)
{
    static const float3 kNormals[6] =
    {
        float3( 0.0f, 0.0f, 1.0f),
        float3( 0.0f, 0.0f,-1.0f),
        float3( 1.0f, 0.0f, 0.0f),
        float3(-1.0f, 0.0f, 0.0f),
        float3( 0.0f, 1.0f, 0.0f),
        float3( 0.0f,-1.0f, 0.0f)
    };
    return kNormals[GetVoxelDirection(voxel)];
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