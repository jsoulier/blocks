#version 450

layout(location = 0) out vec4 color;
layout(set = 3, binding = 0) uniform window_t {
    vec2 size;
} window;
layout(set = 2, binding = 0) uniform sampler2D atlas;

bool in_quad(in vec2 position, in vec2 a, in vec2 b)
{
    return
        position.x > a.x && position.y > a.y &&
        position.x < b.x && position.y < b.y;
}

bool is_crosshair(in vec2 position)
{
    const float w1 = 0.01;
    const float t1 = 0.002;
    const float w2 = w1 / window.size.x * window.size.y;
    const float t2 = t1 / window.size.x * window.size.y;
    const vec2 a1 = vec2(0.5 - w2, 0.5 - t1);
    const vec2 b1 = vec2(0.5 + w2, 0.5 + t1);
    const vec2 a2 = vec2(0.5 - t2, 0.5 - w1);
    const vec2 b2 = vec2(0.5 + t2, 0.5 + w1);
    return
        in_quad(position, a1, b1) ||
        in_quad(position, a2, b2);
}

bool is_block(in vec2 position)
{
    const float left = 0.01;
    const float bottom = 0.01;
    const float width = 0.1 / window.size.x * window.size.y;
    const float height = 0.1;
    const vec2 a = vec2(left, bottom);
    const vec2 b = a + vec2(width, height);
    return in_quad(position, a, b);
}

void main()
{
    vec2 position = gl_FragCoord.xy / window.size;
    position.y = 1.0 - position.y;
    if (is_crosshair(position)) {
        color = vec4(1.0, 1.0, 1.0, 1.0);
    } else if (is_block(position)) {

        const float left = 0.01;
        const float bottom = 0.01;
        const float width = 0.1 / window.size.x * window.size.y;
        const float height = 0.1;
        const float x = position.x - left;
        const float y = position.y - bottom;
        const float a = x / width;
        const float b = y / height;
        const float c = a / 15.0 + 2.0 * (16.0 / 240.0);
        const float d = 1.0 / 15.0 - b / 15.0;
        color = texture(atlas, vec2(c, d));
    } else {
        discard;
    }
}