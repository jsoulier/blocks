#version 450

#include "config.glsl"

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec4 i_shadow;
layout(location = 3) in float i_fog;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_atlas;
layout(set = 2, binding = 1) uniform sampler2D u_depth;
layout(set = 3, binding = 0) uniform camera_t
{
    vec3 vector;
}
u_camera;

void main()
{
    const vec4 block = texture(u_atlas, i_uv);
    if (block.a < 0.001)
    {
        discard;
    }
    const vec3 position = i_shadow.xyz / i_shadow.w;
    float shadow = 0.0;
    if (position.x <= 1.0 && position.x >= 0.0 &&
        position.y <= 1.0 && position.y >= 0.0 &&
        position.z <= 1.0 && position.z >= 0.0 &&
        position.z - world_depth_bias > texture(u_depth, position.xy).r)
    {
        const vec2 size = 1.0 / textureSize(u_depth, 0);
        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                const vec2 uv = position.xy + vec2(x, y) * size;
                const float depth = texture(u_depth, uv).r; 
                shadow += position.z - world_depth_bias < depth ? 0.0 : world_shadow;
            }    
        }
        shadow /= 9.0;
    }
    const float angle = max(dot(i_normal, -u_camera.vector), 0.0);
    const vec3 diffuse = angle * world_light_color;
    const vec3 lighting = world_ambient_color + diffuse + (world_shadow - shadow);
    const vec3 final = block.rgb * lighting;
    o_color = mix(vec4(final, block.a), vec4(sky_bottom_color, 1.0), i_fog);
}