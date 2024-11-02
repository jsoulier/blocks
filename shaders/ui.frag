#version 450

#include "config.glsl"

layout(location = 0) out vec4 color;
layout(set = 2, binding = 0) uniform sampler2D atlas;
layout(set = 3, binding = 0) uniform viewport_t
{
    ivec2 size;
}
viewport;
layout(set = 3, binding = 1) uniform block_t
{
    ivec2 uv;
}
block;

bool draw_crosshair(in vec2 position, in float aspect)
{
    const float width1 = crosshair_width;
    const float thickness1 = crosshair_thickness;
    const float width2 = crosshair_width / aspect;
    const float thickness2 = crosshair_thickness / aspect;
    const vec2 start1 = vec2(0.5 - width2, 0.5 - thickness1);
    const vec2 end1 = vec2(0.5 + width2, 0.5 + thickness1);
    const vec2 start2 = vec2(0.5 - thickness2, 0.5 - width1);
    const vec2 end2 = vec2(0.5 + thickness2, 0.5 + width1);
    if ((position.x > start1.x && position.y > start1.y &&
            position.x < end1.x && position.y < end1.y) ||
        (position.x > start2.x && position.y > start2.y &&
            position.x < end2.x && position.y < end2.y))
    {
        color = vec4(crosshair_color, 1.0);
        return true;
    }
    return false;
}

bool draw_block(in vec2 position, in float aspect)
{
    const float width = block_width / aspect;
    const float height = block_width;
    const vec2 start = vec2(block_left, block_bottom);
    const vec2 end = start + vec2(width, height);
    if (position.x > start.x && position.y > start.y &&
            position.x < end.x && position.y < end.y)
    {
        const float x = (position.x - block_left) / width;
        const float y = (position.y - block_bottom) / height;
        const float u = float(block.uv.x) * ATLAS_FACE_WIDTH / ATLAS_WIDTH;
        const float v = float(block.uv.y) * ATLAS_FACE_HEIGHT / ATLAS_HEIGHT;
        const float c = u + x / ATLAS_X_FACES;
        const float d = v + (1.0 - y) / ATLAS_Y_FACES;
        color = texture(atlas, vec2(c, d));
        return true;
    }
    return false;
}

void main()
{
    vec2 position = gl_FragCoord.xy / viewport.size;
    position.y = 1.0 - position.y;
    const float aspect = float(viewport.size.x) / float(viewport.size.y);
    if (draw_crosshair(position, aspect))
    {
        return;
    }
    else if (draw_block(position, aspect))
    {
        return;
    }
    else
    {
        discard;
    }
}