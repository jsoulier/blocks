cbuffer UniformBuffer : register(b0, space1)
{
    float4x4 Transform : packoffset(c0);
};

cbuffer UniformBuffer : register(b1, space1)
{
    int3 BlockPosition;
};

struct Output
{
    float4 Position : SV_POSITION;
};

static const float3 kVertices[8] =
{
    float3(-0.5f,-0.5f,-0.5f),
    float3( 0.5f,-0.5f,-0.5f),
    float3( 0.5f, 0.5f,-0.5f),
    float3(-0.5f, 0.5f,-0.5f),
    float3(-0.5f,-0.5f, 0.5f),
    float3( 0.5f,-0.5f, 0.5f),
    float3( 0.5f, 0.5f, 0.5f),
    float3(-0.5f, 0.5f, 0.5f) 
};

static const uint kIndices[36] =
{
    0, 1, 2, 0, 2, 3,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    1, 5, 6, 1, 6, 2,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0
};

static const float kScale = 1.01f;

Output main(uint vertexID : SV_VertexID)
{
    Output output;
    float3 position = BlockPosition + 0.5f + kVertices[kIndices[vertexID]] * kScale;
    output.Position = mul(Transform, float4(position, 1.0f));
    return output;
}
