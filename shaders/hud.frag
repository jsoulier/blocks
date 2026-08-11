// TODO: https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/hud.inc"
#include "font.hlsl"

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
static const float kTextAlpha = 0.95f;
static const float kTextWidth = 0.80f;
static const float3 kColor = float3(0.86f, 0.88f, 0.92f);
static const uint kLabelLongest = 6;
static const uint kLabelLengths[HUD_BUTTON_COUNT] = {5, 5, 4, 6, 1, 3, 1};
static const uint2 kLabels[HUD_BUTTON_COUNT] =
{
    uint2(0x43414C50u, 0x00000045u), // PLACE
    uint2(0x41455242u, 0x0000004Bu), // BREAK
    uint2(0x504D554Au, 0x00000000u), // JUMP
    uint2(0x49525053u, 0x0000544Eu), // SPRINT
    uint2(0x0000003Eu, 0x00000000u), // >
    uint2(0x00594C46u, 0x00000000u), // FLY
    uint2(0x0000003Cu, 0x00000000u), // <
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
    uint index = input.Instance - HUD_INSTANCE_BUTTON;
    float texels = kLabelLongest * kFontExtent / kTextWidth;
    if (GetGlyph(kLabels[index], kLabelLengths[index], texels, input.Texcoord))
    {
        return float4(kColor, kTextAlpha);
    }
    else
    {
        return float4(kColor, lerp(kFillAlpha, kEdgeAlpha, edge) * mask);
    }
}
