// https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/hud.inc"

Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    uint Block;
};

struct Input
{
    float2 Texcoord : TEXCOORD0;
    nointerpolation uint Instance : TEXCOORD1;
};

static const float kEpsilon = 0.001f;
static const float kRadius = 0.5f;
static const float kEdge = 0.035f;
static const float kFillAlpha = 0.12f;
static const float kEdgeAlpha = 0.75f;
static const float3 kColors[HUD_BUTTON_COUNT] =
{
    float3(0.45f, 0.85f, 0.50f),
    float3(0.95f, 0.45f, 0.45f),
    float3(0.45f, 0.75f, 0.95f),
    float3(0.95f, 0.80f, 0.40f),
    float3(0.72f, 0.74f, 0.78f),
    float3(0.75f, 0.55f, 0.95f),
};

float4 main(Input input) : SV_Target0
{
    if (input.Instance == HUD_INSTANCE_CROSSHAIR_X || input.Instance == HUD_INSTANCE_CROSSHAIR_Y)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (input.Instance == HUD_INSTANCE_BLOCK)
    {
        float4 color = atlasTexture.Sample(atlasSampler, float3(input.Texcoord, Block));
        if (color.a <= kEpsilon)
        {
            discard;
        }
        return color;
    }
    float distance = length(input.Texcoord - 0.5f);
    float width = fwidth(distance);
    if (distance > kRadius)
    {
        discard;
    }
    float mask = 1.0f - smoothstep(kRadius - width, kRadius, distance);
    float edge = smoothstep(kRadius - kEdge - width, kRadius - kEdge, distance);
    float3 color = kColors[input.Instance - HUD_INSTANCE_BUTTON];
    return float4(color, lerp(kFillAlpha, kEdgeAlpha, edge) * mask);
}
