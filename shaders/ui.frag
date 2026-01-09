Texture2DArray<float4> atlasTexture : register(t0, space2);
SamplerState atlasSampler : register(s0, space2);

cbuffer UniformBuffer : register(b0, space3)
{
    int2 Viewport;
};

cbuffer UniformBuffer : register(b1, space3)
{
    uint Index;
};

struct Output
{
    float4 Color : SV_Target0;
    uint Voxel : SV_Target1;
};

static const float kEpsilon = 0.001f;
static const float kWidth = 1280.0f;
static const float kHeight = 720.0f;

Output main(float4 position : SV_Position)
{
    Output output;
    output.Color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Voxel = 0;
    float2 pixel = float2(position.x, Viewport.y - position.y);
    float2 ratio = float2(Viewport) / float2(kWidth, kHeight);
    float scale = max(ratio.x, ratio.y);
    float blockWidth = 50.0f * scale;
    float2 blockStart = float2(10.0f * scale, 10.0f * scale);
    float2 blockEnd = blockStart + blockWidth;
    if (pixel.x > blockStart.x && pixel.x < blockEnd.x &&
        pixel.y > blockStart.y && pixel.y < blockEnd.y)
    {
        float x = (pixel.x - blockStart.x) / blockWidth;
        float y = (pixel.y - blockStart.y) / blockWidth;
        output.Color = atlasTexture.Sample(atlasSampler, float3(x, 1.0f - y, Index));
        if (output.Color.a > kEpsilon)
        {
            return output;
        }
    }
    float crossWidth = 8.0f * scale;
    float crossThickness = 2.0f * scale;
    float2 crossCenter = float2(Viewport) * 0.5f;
    float2 crossStart1 = crossCenter - float2(crossWidth, crossThickness);
    float2 crossEnd1 = crossCenter + float2(crossWidth, crossThickness);
    float2 crossStart2 = crossCenter - float2(crossThickness, crossWidth);
    float2 crossEnd2 = crossCenter + float2(crossThickness, crossWidth);
    if ((pixel.x > crossStart1.x && pixel.y > crossStart1.y &&
         pixel.x < crossEnd1.x   && pixel.y < crossEnd1.y) ||
        (pixel.x > crossStart2.x && pixel.y > crossStart2.y &&
         pixel.x < crossEnd2.x   && pixel.y < crossEnd2.y))
    {
        output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        return output;
    }
    discard;
    return output;
}
