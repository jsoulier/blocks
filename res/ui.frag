#version 450

#include "helpers.glsl"

#define CROSSHAIR_WIDTH 0.01
#define CROSSHAIR_THICKNESS 0.002
#define BLOCK_LEFT 0.01
#define BLOCK_BOTTOM 0.01
#define BLOCK_WIDTH 0.1
#define ALPHA 0.6

layout(location = 0) out vec4 color;
layout(set = 3, binding = 0) uniform window_t {
    vec2 size;
} window;
layout(set = 3, binding = 1) uniform block_t {
    ivec2 uv;
} block;
layout(set = 2, binding = 0) uniform sampler2D atlas;

bool draw_crosshair(in vec2 position)
{
    const float aspect = window.size.x / window.size.y;
    const float width_1 = CROSSHAIR_WIDTH;
    const float thickness_1 = CROSSHAIR_THICKNESS;
    const float width_2 = CROSSHAIR_WIDTH / aspect;
    const float thickness_2 = CROSSHAIR_THICKNESS / aspect;
    const vec2 start1 = vec2(0.5 - width_2, 0.5 - thickness_1);
    const vec2 end1 = vec2(0.5 + width_2, 0.5 + thickness_1);
    const vec2 start_2 = vec2(0.5 - thickness_2, 0.5 - width_1);
    const vec2 end2 = vec2(0.5 + thickness_2, 0.5 + width_1);
    if ((position.x > start1.x && position.y > start1.y &&
            position.x < end1.x && position.y < end1.y) ||
        (position.x > start_2.x && position.y > start_2.y &&
            position.x < end2.x && position.y < end2.y)) {
        color = vec4(1.0, 1.0, 1.0, ALPHA);
        return true;
    }
    return false;
}

bool draw_block(in vec2 position)
{
    const float aspect = window.size.x / window.size.y;
    const float width = BLOCK_WIDTH / aspect;
    const float height = BLOCK_WIDTH;
    const vec2 start = vec2(BLOCK_LEFT, BLOCK_BOTTOM);
    const vec2 end = start + vec2(width, height);
    if (position.x > start.x && position.y > start.y &&
            position.x < end.x && position.y < end.y) {
        const float x = (position.x - BLOCK_LEFT) / width;
        const float y = (position.y - BLOCK_BOTTOM) / height;
        const float u = float(block.uv.x) * ATLAS_FACE_WIDTH / ATLAS_WIDTH;
        const float v = float(block.uv.y) * ATLAS_FACE_HEIGHT / ATLAS_HEIGHT;
        const float c = u + x / ATLAS_X_FACES;
        const float d = v + y / ATLAS_Y_FACES;
        const float flipped = 1.0 / ATLAS_Y_FACES - d;
        color = texture(atlas, vec2(c, flipped));
        return true;
    }
    return false;
}

void main()
{
    vec2 position = gl_FragCoord.xy / window.size;
    position.y = 1.0 - position.y;
    if (draw_crosshair(position)) {
        return;
    } else if (draw_block(position)) {
        return;
    }
    discard;
}