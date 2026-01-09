struct Output
{
    float4 Color : SV_Target0;
    uint Voxel : SV_Target1;
};

Output main()
{
    Output output;
    output.Color = float4(1.0f, 1.0f, 1.0f, 0.1f);
    output.Voxel = 0;
    return output;
}
