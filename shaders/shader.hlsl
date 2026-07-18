#ifndef SHADER_HLSL
#define SHADER_HLSL

// https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/voxel.inc"

static const float kEpsilon = 0.001f;
static const float kPi = 3.14159265f;

static const float3 kNormals[10] =
{
    float3( 0.0f, 0.0f, 1.0f ),
    float3( 0.0f, 0.0f,-1.0f ),
    float3( 1.0f, 0.0f, 0.0f ),
    float3(-1.0f, 0.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
    float3( 0.0f,-1.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
};

static const float3 kCubePositions[8] =
{
    float3(-0.5f,-0.5f,-0.5f ),
    float3( 0.5f,-0.5f,-0.5f ),
    float3( 0.5f, 0.5f,-0.5f ),
    float3(-0.5f, 0.5f,-0.5f ),
    float3(-0.5f,-0.5f, 0.5f ),
    float3( 0.5f,-0.5f, 0.5f ),
    float3( 0.5f, 0.5f, 0.5f ),
    float3(-0.5f, 0.5f, 0.5f ),
};

static const float3 kCubeNormals[6] =
{
    float3( 0.0f, 0.0f,-1.0f ),
    float3( 0.0f, 0.0f, 1.0f ),
    float3(-1.0f, 0.0f, 0.0f ),
    float3( 1.0f, 0.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ),
    float3( 0.0f,-1.0f, 0.0f ),
};

static const uint kCubeIndices[36] =
{
    0, 1, 2, 0, 2, 3,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    1, 5, 6, 1, 6, 2,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0
};

float GetAO(uint voxel)
{
    static const float kAO[4] = {0.4f, 0.6f, 0.8f, 1.0f};
    return kAO[(voxel >> AO_OFFSET) & AO_MASK];
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
    return kCubePositions[kCubeIndices[vertexID]];
}

float3 GetCubeNormal(uint vertexID)
{
    return kCubeNormals[vertexID / 6];
}

bool IsSky(uint voxel)
{
    return voxel == 0;
}

bool IsCloud(float3 color)
{
    return length(color) > (1.0f - kEpsilon);
}

struct Light
{
    uint Color;
    int X;
    int Y;
    int Z;
};

float3 GetDiffuseLight(StructuredBuffer<Light> lights, uint lightCount, float4 position, float3 normal)
{
    static const float3 kOffset = float3(0.0f, 0.25f, 0.0f);
    static const float kLight = 2.0f;
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < lightCount; i++)
    {
        Light light = lights[i];
        float radius = (light.Color & 0xFF000000) >> 24;
        float3 lightPosition = float3(light.X, light.Y, light.Z) + 0.5f + kOffset;
        float3 offset = lightPosition - position.xyz;
        float distance = length(offset);
        if (distance >= radius || radius <= 0.0f)
        {
            continue;
        }
        float3 lightDirection = offset / distance;
        float NdotL = saturate(dot(normal, lightDirection));
        if (NdotL <= 0.0f)
        {
            continue;
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

float GetSunLight(Texture2D<float> texture, SamplerState state, float4x4 transform, float3 position, float3 normal, uint voxel)
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
    if (GetShadow(voxel))
    {
        ratio = dot(normal, direction);
        if (ratio > 0.0f)
        {
            return 0.0f;
        }
    }
    float depth = shadowPosition.z;
    float closestDepth = texture.SampleLevel(state, uv, 0);
    if (depth < closestDepth + kBias)
    {
        return kBase - kShadow * ratio;
    }
    else
    {
        return 0.0f;
    }
}

float3 GetAmbientLight()
{
    return float3(0.5f, 0.5f, 0.5f);
}

float GetFog(float x)
{
    return min(pow(x / 250.0f, 2.5f), 1.0f);
}

float3 GetSkyColor(float3 position)
{
    static const float3 kTop = float3(0.212f, 0.773f, 0.957f);
    static const float3 kBottom = float3(0.220f, 0.349f, 0.702f);
    float dy = position.y;
    float dx = length(float2(position.x, position.z));
    float alpha = (atan2(dy, dx) + kPi / 2.0f) / kPi;
    return lerp(kBottom, kTop, alpha);
}

#endif
