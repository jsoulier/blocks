#version 450

#include "config.glsl"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in float fog;
layout(location = 3) in vec4 shadow_position;
layout(location = 0) out vec4 color;
layout(set = 2, binding = 0) uniform sampler2D atlas;
layout(set = 2, binding = 1) uniform sampler2D shadow_map;

void main()
{
    vec4 base = texture(atlas, uv);
    if (base.a < 0.001)
    {
        discard;
    }
    vec3 shadow_coords = shadow_position.xyz / shadow_position.w;
    // vec3 shadow_coords = shadow_position.xyz;

    shadow_coords.y = 1.0 - shadow_coords.y;
    float shadow;
    if(shadow_coords.z > 1.0 || shadow_coords.z < 0.0)
    {
        shadow = 0.0;
    }
    else if (shadow_coords.x > 1.0 || shadow_coords.x < 0.0)
    {
        shadow = 0.0;
    }
    else if (shadow_coords.y > 1.0 || shadow_coords.y < 0.0)
    {
        shadow = 0.0;
    }
    else
    {
        // TODO: slope bias
        float min_depth = texture(shadow_map, shadow_coords.xy).r; 
        float depth = shadow_coords.z;
        shadow = (depth - 0.001f) < min_depth ? 0.0 : 0.5;
    }
    float angle = max(dot(normalize(normal), -light_direction), 0.0);
    vec3 diffuse = angle * light_color;
    vec3 lighting = ambient_color + diffuse + (0.5 - shadow);
    vec3 final_color = base.rgb * lighting;
    color = mix(vec4(final_color, base.a), vec4(sky_color, 1.0), fog);
}