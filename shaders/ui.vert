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

static const float2 kReferenceSize = float2(1280.0f, 720.0f);

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
    float2 viewportScale = float2(Viewport) / kReferenceSize;
    float scale = max(viewportScale.x, viewportScale.y);
    float2 minimum;
    float2 maximum;
    if (instanceID == 0)
    {
        minimum = float2(10.0f, 10.0f) * scale;
        maximum = minimum + 50.0f * scale;
    }
    else
    {
        float2 center = float2(Viewport) * 0.5f;
        float2 size = instanceID == 1 ? float2(8.0f, 2.0f) : float2(2.0f, 8.0f);
        minimum = center - size * scale;
        maximum = center + size * scale;
    }
    float2 position = lerp(minimum, maximum, kPositions[vertexID]);
    position = position / float2(Viewport) * 2.0f - 1.0f;
    Output output;
    output.Position = float4(position, 0.0f, 1.0f);
    output.Texcoord = float2(kPositions[vertexID].x, 1.0f - kPositions[vertexID].y);
    output.Index = Index;
    output.IsTexture = instanceID == 0;
    return output;
}
