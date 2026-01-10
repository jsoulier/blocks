#ifndef CUBE_HLSL
#define CUBE_HLSL

static const float3 kCubeVertices[8] =
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

static const uint kCubeIndices[36] =
{
    0, 1, 2, 0, 2, 3,
    5, 4, 7, 5, 7, 6,
    4, 0, 3, 4, 3, 7,
    1, 5, 6, 1, 6, 2,
    3, 2, 6, 3, 6, 7,
    4, 5, 1, 4, 1, 0
};

#endif