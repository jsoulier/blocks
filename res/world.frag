#version 450

#include "config.glsl"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in float fog;
layout(location = 0) out vec4 color;
layout(set = 2, binding = 0) uniform sampler2D atlas;

void main()
{
    vec4 base = texture(atlas, uv);
    if (base.a < 0.001)
    {
        discard;
    }
    float angle = max(dot(normalize(normal), -light_direction), 0.0);
    vec3 diffuse = angle * light_color;
    vec3 lighting = ambient_color + diffuse;
    vec3 final_color = base.rgb * lighting;
    color = mix(vec4(final_color, base.a), vec4(sky_color, 1.0), fog);
}