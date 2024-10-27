#version 450

layout(location = 0) out vec4 color;
layout(set = 3, binding = 0) uniform window_t {
    vec2 size;
} window;
layout(set = 3, binding = 1) uniform camera_t {
    float pitch;
} camera;

vec4 sky = vec4(0.5, 0.7, 1.0, 1.0);
vec4 horizon = vec4(0.2, 0.4, 0.7, 1.0);

void main()
{
    float pitch = (sin(camera.pitch) + 1.0) / 2.0;
    float ndc = gl_FragCoord.y / window.size.y;
    color = mix(horizon, sky, ndc - pitch / 1.25);
}