#ifndef VOXEL_HLSL
#define VOXEL_HLSL

#include "voxel.inc"

static const float3 kVoxelNormals[6] =
{
    float3( 0, 0, 1),
    float3( 0, 0,-1),
    float3( 1, 0, 0),
    float3(-1, 0, 0),
    float3( 0, 1, 0),
    float3( 0,-1, 0)
};

float3 VoxelGetPosition(uint voxel)
{
    return float3(
        (voxel >> VOXEL_X_OFFSET) & VOXEL_X_MASK,
        (voxel >> VOXEL_Y_OFFSET) & VOXEL_Y_MASK,
        (voxel >> VOXEL_Z_OFFSET) & VOXEL_Z_MASK);
}

float3 VoxelGetTexcoord(uint voxel)
{
    return float3(
        (voxel >> VOXEL_U_OFFSET) & VOXEL_U_MASK,
        (voxel >> VOXEL_V_OFFSET) & VOXEL_V_MASK,
        (voxel >> VOXEL_FACE_OFFSET) & VOXEL_FACE_MASK);
}

uint VoxelGetDirection(uint voxel)
{
    return (voxel >> VOXEL_DIRECTION_OFFSET) & VOXEL_DIRECTION_MASK;
}

uint VoxelGetOcclusion(uint voxel)
{
    return (voxel >> VOXEL_OCCLUSION_OFFSET) & VOXEL_OCCLUSION_MASK;
}

float3 VoxelGetNormal(uint voxel)
{
    return kVoxelNormals[VoxelGetDirection(voxel)];
}

#endif