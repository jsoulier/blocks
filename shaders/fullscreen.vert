struct Output
{
    float2 Texcoord : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(uint vertexID : SV_VertexID)
{
    Output output;
    output.Texcoord = float2(float((vertexID << 1) & 2), float(vertexID & 2));
    output.Position = float4((output.Texcoord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
