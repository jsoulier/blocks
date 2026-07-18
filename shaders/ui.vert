cbuffer UniformBuffer : register(b0, space1)
{
    int2 Viewport;
    uint Index;
};

struct Output
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD0;
    nointerpolation uint Index : TEXCOORD1;
    nointerpolation uint IsTexture : TEXCOORD2;
};

static const float kWidth = 1280.0f;
static const float kHeight = 720.0f;

static const float2 kPositions[6] = {
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

Output main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    float scale = max(Viewport.x / kWidth, Viewport.y / kHeight);
    float2 start;
    float2 end;
    if (instanceID == 0)
    {
        start = float2(10.0f, 10.0f) * scale;
        end = start + 50.0f * scale;
    }
    else
    {
        float2 center = float2(Viewport) * 0.5f;
        float2 size = instanceID == 1 ? float2(8.0f, 2.0f) : float2(2.0f, 8.0f);
        start = center - size * scale;
        end = center + size * scale;
    }
    float2 position = lerp(start, end, kPositions[vertexID]);
    position = position / float2(Viewport) * 2.0f - 1.0f;
    Output output;
    output.Position = float4(position, 0.0f, 1.0f);
    output.Texcoord = float2(kPositions[vertexID].x, 1.0f - kPositions[vertexID].y);
    output.Index = Index;
    output.IsTexture = instanceID == 0;
    return output;
}
