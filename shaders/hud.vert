// TODO: https://github.com/libsdl-org/SDL_shadercross/issues/211
#include "../src/hud.inc"

cbuffer UniformBuffer : register(b0, space1)
{
    int2 Viewport;
    int Touch;
    int Padding;
    int4 Safe;
};

struct Input
{
    uint VertexID : SV_VertexID;
    uint InstanceID : SV_InstanceID;
};

struct Output
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD0;
    nointerpolation uint Instance : TEXCOORD1;
};

static const float2 kPositions[6] =
{
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

static const float2 kButtons[HUD_BUTTON_COUNT] =
{
    float2(HUD_BUTTON_PLACE_X, HUD_BUTTON_PLACE_Y),
    float2(HUD_BUTTON_BREAK_X, HUD_BUTTON_BREAK_Y),
    float2(HUD_BUTTON_JUMP_X, HUD_BUTTON_JUMP_Y),
    float2(HUD_BUTTON_SPRINT_X, HUD_BUTTON_SPRINT_Y),
    float2(HUD_BUTTON_BLOCK_NEXT_X, HUD_BUTTON_BLOCK_NEXT_Y),
    float2(HUD_BUTTON_CONTROLLER_X, HUD_BUTTON_CONTROLLER_Y),
    float2(HUD_BUTTON_BLOCK_PREV_X, HUD_BUTTON_BLOCK_PREV_Y),
};

static float2 GetCenter(uint index, float4 safe, float scale)
{
    float2 offset = kButtons[index];
    float x = HUD_BUTTON_CENTER_X(safe.x, safe.z, index, offset.x, scale);
    float y = HUD_BUTTON_CENTER_Y(safe.y, offset.y, scale);
    return float2(x, y);
}

Output main(Input input)
{
    float2 viewport = float2(Viewport);
    float4 safe = float4(Safe);
    float scale = HUD_SCALE(viewport.x, viewport.y);
    float2 minimum;
    float2 maximum;
    Output output;
    output.Instance = input.InstanceID;
    if (input.InstanceID == HUD_INSTANCE_CROSSHAIR_X || input.InstanceID == HUD_INSTANCE_CROSSHAIR_Y)
    {
        float2 center = viewport * 0.5f;
        float2 size = float2(HUD_CROSSHAIR_WIDTH, HUD_CROSSHAIR_HEIGHT);
        size = input.InstanceID == HUD_INSTANCE_CROSSHAIR_X ? size.xy : size.yx;
        minimum = center - size * scale;
        maximum = center + size * scale;
    }
    else if (input.InstanceID == HUD_INSTANCE_BLOCK)
    {
        minimum = safe.xy + HUD_BLOCK_MARGIN * scale;
        maximum = minimum + HUD_BLOCK_SIZE * scale;
    }
    else if (!Touch)
    {
        minimum = float2(0.0f, 0.0f);
        maximum = float2(0.0f, 0.0f);
    }
    else
    {
        float2 center = GetCenter(input.InstanceID - HUD_INSTANCE_BUTTON, safe, scale);
        minimum = center - HUD_BUTTON_RADIUS * scale;
        maximum = center + HUD_BUTTON_RADIUS * scale;
    }
    float2 position = lerp(minimum, maximum, kPositions[input.VertexID]);
    position = position / viewport * 2.0f - 1.0f;
    output.Position = float4(position, 0.0f, 1.0f);
    output.Texcoord = float2(kPositions[input.VertexID].x, 1.0f - kPositions[input.VertexID].y);
    return output;
}
