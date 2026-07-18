#ifndef SHADER_HLSL
#define SHADER_HLSL

// https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/voxel.inc"

static const float kEpsilon = 0.001f;
static const float kPi = 3.14159265f;

struct Block
{
    uint IsOpaque;
    uint IsSolid;
    uint IsSprite;
    uint IsOccluded;
    uint CanCreateShadow;
    uint CanBeInShadow;
    float SunIntensity;
    uint LightColor;
    int LightX;
    int LightY;
    int LightZ;
    uint Indices[6];
};

static const float3 kNormals[6] = {
    float3(0.0f, 0.0f, 1.0f),  // north
    float3(0.0f, 0.0f, -1.0f), // south
    float3(1.0f, 0.0f, 0.0f),  // east
    float3(-1.0f, 0.0f, 0.0f), // west
    float3(0.0f, 1.0f, 0.0f),  // up
    float3(0.0f, -1.0f, 0.0f), // down
};

static const float3 kCubePositions[8] = {
    float3(-0.5f, -0.5f, -0.5f), // -x, -y, -z
    float3(0.5f, -0.5f, -0.5f),  // +x, -y, -z
    float3(0.5f, 0.5f, -0.5f),   // +x, +y, -z
    float3(-0.5f, 0.5f, -0.5f),  // -x, +y, -z
    float3(-0.5f, -0.5f, 0.5f),  // -x, -y, +z
    float3(0.5f, -0.5f, 0.5f),   // +x, -y, +z
    float3(0.5f, 0.5f, 0.5f),    // +x, +y, +z
    float3(-0.5f, 0.5f, 0.5f),   // -x, +y, +z
};

static const float3 kCubeNormals[6] = {
    float3(0.0f, 0.0f, -1.0f), // -z
    float3(0.0f, 0.0f, 1.0f),  // +z
    float3(-1.0f, 0.0f, 0.0f), // -x
    float3(1.0f, 0.0f, 0.0f),  // +x
    float3(0.0f, 1.0f, 0.0f),  // +y
    float3(0.0f, -1.0f, 0.0f), // -y
};

static const uint kCubeIndices[36] = {
    0, 1, 2, 0, 2, 3, // -z
    5, 4, 7, 5, 7, 6, // +z
    4, 0, 3, 4, 3, 7, // -x
    1, 5, 6, 1, 6, 2, // +x
    3, 2, 6, 3, 6, 7, // +y
    4, 5, 1, 4, 1, 0, // -y
};

static const float kAO[4] = {0.4f, 0.6f, 0.8f, 1.0f};

float GetOcclusion(uint voxel)
{
    return kAO[(voxel >> VOXEL_AO_OFFSET) & VOXEL_AO_MASK];
}

uint GetDirection(uint voxel)
{
    return (voxel >> VOXEL_DIRECTION_OFFSET) & VOXEL_DIRECTION_MASK;
}

uint GetBlockIndex(uint voxel)
{
    return (voxel >> VOXEL_BLOCK_OFFSET) & VOXEL_BLOCK_MASK;
}

float3 GetPosition(uint voxel)
{
    return float3(
        (voxel >> VOXEL_X_OFFSET) & VOXEL_X_MASK,
        (voxel >> VOXEL_Y_OFFSET) & VOXEL_Y_MASK,
        (voxel >> VOXEL_Z_OFFSET) & VOXEL_Z_MASK);
}

uint GetAtlasIndex(uint voxel, Block block)
{
    return block.Indices[GetDirection(voxel)];
}

float2 GetTexcoord(uint voxel)
{
    return float2((voxel >> VOXEL_U_OFFSET) & VOXEL_U_MASK, (voxel >> VOXEL_V_OFFSET) & VOXEL_V_MASK);
}

float3 GetNormal(uint voxel)
{
    return kNormals[GetDirection(voxel)];
}

float3 GetCubePosition(uint vertexID)
{
    return kCubePositions[kCubeIndices[vertexID]];
}

float3 GetCubeNormal(uint vertexID)
{
    return kCubeNormals[vertexID / 6];
}

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

float3 GetPointLight(StructuredBuffer<Light> lights, uint lightCount, float3 position, float3 normal)
{
    static const float3 kOffset = float3(0.0f, 0.25f, 0.0f);
    float3 accumulatedLight = float3(0.0f, 0.0f, 0.0f);
    for (uint lightIndex = 0; lightIndex < lightCount; lightIndex++)
    {
        Light light = lights[lightIndex];
        float radius = (light.Color & 0xFF000000) >> 24;
        float3 lightPosition = float3(light.X, light.Y, light.Z) + 0.5f + kOffset;
        float3 offset = lightPosition - position;
        float distance = length(offset);
        if (distance >= radius)
        {
            continue;
        }
        float3 lightDirection = offset / distance;
        float lightAngle = saturate(dot(normal, lightDirection));
        if (lightAngle <= 0.0f)
        {
            continue;
        }
        float normalizedDistance = saturate(distance / radius);
        float attenuation = 1.0f - smoothstep(0.0f, 1.0f, normalizedDistance);
        float3 lightColor;
        lightColor.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        lightColor.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        lightColor.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        accumulatedLight += lightColor * lightAngle * attenuation;
    }
    return accumulatedLight;
}

float GetSunlight(
    Texture2D<float> shadowTexture,
    SamplerComparisonState shadowSampler,
    float4x4 shadowTransform,
    float3 sunDirection,
    float sunIntensity,
    float3 position,
    float3 normal,
    Block block)
{
    if (sunIntensity <= 0.0f)
    {
        return 0.0f;
    }
    float sunlight = block.SunIntensity * sunIntensity;
    if (!block.CanBeInShadow)
    {
        return sunlight;
    }
    float lightAngle = block.IsOccluded ? saturate(-dot(normal, sunDirection)) : 0.707f;
    if (lightAngle <= 0.0f)
    {
        return 0.0f;
    }
    float4 shadowPosition = mul(shadowTransform, float4(position, 1.0f));
    shadowPosition.xyz /= shadowPosition.w;
    float2 uv = shadowPosition.xy * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || shadowPosition.z < 0.0f || shadowPosition.z > 1.0f)
    {
        return sunlight * lightAngle;
    }
    uint width;
    uint height;
    shadowTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float bias = 0.0003f + 0.001f * (1.0f - lightAngle);
    float shadowVisibility = 0.0f;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            shadowVisibility +=
                shadowTexture.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texelSize, shadowPosition.z - bias);
        }
    }
    shadowVisibility /= 9.0f;
    float shadowFade = smoothstep(0.05f, 0.2f, sunIntensity);
    shadowVisibility = lerp(1.0f, shadowVisibility, shadowFade);
    return sunlight * lightAngle * shadowVisibility;
}

float GetFogValue(float distance)
{
    return min(pow(distance / 250.0f, 2.5f), 1.0f);
}

float3 GetSkyColor(float3 position, float3 top, float3 horizon)
{
    return lerp(horizon, top, (atan2(position.y, length(position.xz)) + kPi / 2.0f) / kPi);
}

#endif
