#ifndef SKY_HLSL
#define SKY_HLSL

float3 SkyGetColor(float3 position, float3 color1, float3 color2)
{
    static const float kPi = 3.14159265f;
    float dy = position.y;
    float dx = length(float2(position.x, position.z));
    float pitch = atan2(dy, dx);
    float alpha = (pitch + kPi / 2.0f) / kPi;
    return lerp(color1, color2, alpha);
}

#endif