#version 450

layout(location = 0) out vec4 color;
layout(set = 3, binding = 0) uniform window_t {
     vec2 size;
} window;

const float crosshair_size = 0.02f;
const float crosshair_thickness = 0.005f;
const vec4 crosshair_color = vec4(0.9, 0.9, 0.9, 1.0);

bool is_crosshair(in vec2 ndc)
{
    return
        (abs(ndc.x) < crosshair_size &&
            abs(ndc.y) < crosshair_thickness) ||
        (abs(ndc.y) < crosshair_size &&
            abs(ndc.x) < crosshair_thickness);
}

void main()
{
    vec2 ndc = (gl_FragCoord.xy / window.size) * 2.0 - 1.0;
    ndc.x *= window.size.x / window.size.y;
    if (!is_crosshair(ndc)) {
        discard;
    }
    color = crosshair_color;
}