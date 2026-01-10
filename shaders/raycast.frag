struct Output
{
    float4 Color : SV_Target0;
    float4 Position : SV_Target1;
    float4 Light : SV_Target2;
    uint Voxel : SV_Target3;
};

Output main()
{
    Output output;
    output.Color = float4(1.0f, 1.0f, 1.0f, 0.1f);
    output.Voxel = 0;
    return output;
}
