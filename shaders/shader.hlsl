#ifndef SHADER_HLSL
#define SHADER_HLSL

// https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/voxel.inc"

static const float kEpsilon = 0.001f;
static const float kPi = 3.14159265f;
static const uint kWater = 16;

struct Light
{
    int3 Position;
    uint Color;
};

struct Block
{
    Light _Light;
    uint IsOpaque;
    uint IsSolid;
    uint IsSprite;
    uint UseAO;
    uint UseSunNormal;
    float SunIntensity;
    uint Indices[6];
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

static const uint kCubeIndices[36] =
{
    0, 1, 2, 0, 2, 3, 
    5, 4, 7, 5, 7, 6, 
    4, 0, 3, 4, 3, 7, 
    1, 5, 6, 1, 6, 2, 
    3, 2, 6, 3, 6, 7, 
    4, 5, 1, 4, 1, 0, 
};

static const float3 kNormals[6] =
{
    float3( 0.0f, 0.0f, 1.0f ), 
    float3( 0.0f, 0.0f,-1.0f ),
    float3( 1.0f, 0.0f, 0.0f ), 
    float3(-1.0f, 0.0f, 0.0f ),
    float3( 0.0f, 1.0f, 0.0f ), 
    float3( 0.0f,-1.0f, 0.0f ),
};

static const float kAO[4] = {0.4f, 0.6f, 0.8f, 1.0f};

uint GetDirection(uint voxel)
{
    return (voxel >> DIRECTION_OFFSET) & DIRECTION_MASK;
}

uint GetBlockIndex(uint voxel)
{
    return (voxel >> BLOCK_OFFSET) & BLOCK_MASK;
}

uint GetAtlasIndex(uint voxel, Block block)
{
    return block.Indices[GetDirection(voxel)];
}

float3 GetPosition(uint voxel)
{
    return float3((voxel >> X_OFFSET) & X_MASK, (voxel >> Y_OFFSET) & Y_MASK, (voxel >> Z_OFFSET) & Z_MASK);
}

float3 GetCubePosition(uint vertexID)
{
    return kCubePositions[kCubeIndices[vertexID]];
}

float2 GetTexcoord(uint voxel)
{
    return float2((voxel >> U_OFFSET) & U_MASK, (voxel >> V_OFFSET) & V_MASK);
}

float3 GetNormal(uint voxel)
{
    return kNormals[GetDirection(voxel)];
}

float GetAO(uint voxel)
{
    return kAO[(voxel >> AO_OFFSET) & AO_MASK];
}

float3 GetLight(StructuredBuffer<Light> lights, uint count, float3 position, float3 normal, Block block)
{
    static const float3 kOffset = float3(0.0f, 0.25f, 0.0f);
    float3 final = float3(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < count; i++)
    {
        Light light = lights[i];
        float radius = (light.Color & 0xFF000000) >> 24;
        float3 offset = light.Position + 0.5f + kOffset - position;
        float distance = length(offset);
        if (distance >= radius)
        {
            continue;
        }
        float angle = 1.0f;
        if (!block.IsSprite)
        {
            angle = saturate(dot(normal, offset / distance));
        }
        if (angle <= 0.0f)
        {
            continue;
        }
        float attenuation = 1.0f - smoothstep(0.0f, 1.0f, saturate(distance / radius));
        float3 color;
        color.r = ((light.Color & 0x000000FF) >> 0) / 255.0f;
        color.g = ((light.Color & 0x0000FF00) >> 8) / 255.0f;
        color.b = ((light.Color & 0x00FF0000) >> 16) / 255.0f;
        final += color * angle * attenuation;
    }
    return final;
}

float GetSunlight(float3 direction, float intensity, float3 normal, Block block)
{
    if (intensity <= 0.0f)
    {
        return 0.0f;
    }
    float angle = 1.0f;
    if (block.UseSunNormal)
    {
        angle = saturate(-dot(normal, direction));
    }
    return block.SunIntensity * intensity * angle;
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
