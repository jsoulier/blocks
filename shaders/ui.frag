#version 450

#include "config.glsl"

layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_atlas;
layout(set = 3, binding = 0) uniform viewport_t
{
    ivec2 size;
}
u_viewport;
layout(set = 3, binding = 1) uniform block_t
{
    ivec2 uv;
}
u_block;

void main()
{
    vec2 position = gl_FragCoord.xy / u_viewport.size;
    position.y = 1.0 - position.y;
    const float aspect = float(u_viewport.size.x) / float(u_viewport.size.y);
    {
        const float width = ui_block_width / aspect;
        const float height = ui_block_width;
        const vec2 start = vec2(ui_block_left, ui_block_bottom);
        const vec2 end = start + vec2(width, height);
        if (position.x > start.x && position.x < end.x &&
            position.y > start.y && position.y < end.y)
        {
            const float x = (position.x - ui_block_left) / width;
            const float y = (position.y - ui_block_bottom) / height;
            const float u = u_block.uv.x * ATLAS_FACE_WIDTH / ATLAS_WIDTH;
            const float v = u_block.uv.y * ATLAS_FACE_HEIGHT / ATLAS_HEIGHT;
            const float c = u + x / ATLAS_X_FACES;
            const float d = v + (1.0 - y) / ATLAS_Y_FACES;
            o_color = texture(u_atlas, vec2(c, d)) * ui_block_light;
            return;
        }
    }
    {
        const float width1 = ui_crosshair_width;
        const float thickness1 = ui_crosshair_thickness;
        const float width2 = ui_crosshair_width / aspect;
        const float thickness2 = ui_crosshair_thickness / aspect;
        const vec2 start1 = vec2(0.5 - width2, 0.5 - thickness1);
        const vec2 end1 = vec2(0.5 + width2, 0.5 + thickness1);
        const vec2 start2 = vec2(0.5 - thickness2, 0.5 - width1);
        const vec2 end2 = vec2(0.5 + thickness2, 0.5 + width1);
        if ((position.x > start1.x && position.y > start1.y &&
            position.x < end1.x && position.y < end1.y) ||
            (position.x > start2.x && position.y > start2.y &&
            position.x < end2.x && position.y < end2.y))
        {
            o_color = ui_crosshair_color;
            return;
        }
    }
    discard;
}