cbuffer UniformBuffer : register(b0, space1)
{
    int2 Viewport;
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
    nointerpolation uint IsBlock : TEXCOORD1;
};

static const float2 kCrosshair = float2(8.0f, 2.0f);
static const float2 kSize = float2(1280.0f, 720.0f);
static const float2 kPositions[6] =
{
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

Output main(Input input)
{
    float2 ratio = float2(Viewport) / kSize;
    float scale = max(ratio.x, ratio.y);
    float2 minimum;
    float2 maximum;
    Output output;
    output.IsBlock = input.InstanceID == 0;
    if (output.IsBlock)
    {
        minimum = float2(10.0f, 10.0f) * scale;
        maximum = minimum + 50.0f * scale;
    }
    else
    {
        float2 center = float2(Viewport) * 0.5f;
        float2 size = input.InstanceID == 1 ? kCrosshair.xy : kCrosshair.yx;
        minimum = center - size * scale;
        maximum = center + size * scale;
    }
    float2 position = lerp(minimum, maximum, kPositions[input.VertexID]);
    position = position / float2(Viewport) * 2.0f - 1.0f;
    output.Position = float4(position, 0.0f, 1.0f);
    output.Texcoord = float2(kPositions[input.VertexID].x, 1.0f - kPositions[input.VertexID].y);
    return output;
}
