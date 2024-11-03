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
    float shadow = 0.0;
    if(shadow_coords.z > 1.0 || shadow_coords.z < 0.0)
    {
        shadow = 0.0;
        color = vec4(1);
        return;
    }
    else if (shadow_coords.x > 1.0 || shadow_coords.x < 0.0)
    {
        shadow = 0.0;
        color = vec4(1);
        return;
    }
    else if (shadow_coords.y > 1.0 || shadow_coords.y < 0.0)
    {
        shadow = 0.0;
        color = vec4(1);
        return;
    }
    else
    {
        // TODO: slope bias
        float min_depth = texture(shadow_map, shadow_coords.xy).r; 
        float depth = shadow_coords.z;
        // shadow = (depth) < min_depth ? 0.0 : 0.5;
        // color = vec4(vec3(depth), 1);
        // return;

        float bias = 0.0005f;
        vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
        for(int x = -1; x <= 1; ++x)
        {
            for(int y = -1; y <= 1; ++y)
            {
                float pcfDepth = texture(shadow_map, shadow_coords.xy + vec2(x, y) * texelSize).r; 
                shadow += depth - bias < pcfDepth ? 0.0 : 0.5;        
            }    
        }
        shadow /= 9.0;
    }
    float angle = max(dot(normalize(normal), -light_direction), 0.0);
    vec3 diffuse = angle * light_color;
    vec3 lighting = ambient_color + diffuse + (0.5 - shadow);
    vec3 final_color = base.rgb * lighting;
    color = mix(vec4(final_color, base.a), vec4(sky_color, 1.0), fog);
}