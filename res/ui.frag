#version 450

#define CROSSHAIR_CENTER 0.5
#define CROSSHAIR_WIDTH 0.01
#define CROSSHAIR_THICKNESS 0.002
#define BLOCK_LEFT 0.01
#define BLOCK_BOTTOM 0.01
#define BLOCK_WIDTH 0.1
#define ATLAS_WIDTH 240.0
#define ATLAS_HEIGHT 240.0
#define ATLAS_FACE_WIDTH 16.0
#define ATLAS_X_FACES 15.0
#define ATLAS_Y_FACES 15.0

layout(location = 0) out vec4 color;
layout(set = 3, binding = 0) uniform window_t {
    vec2 size;
} window;
layout(set = 3, binding = 1) uniform block_t {
    ivec2 uv;
} block;
layout(set = 2, binding = 0) uniform sampler2D atlas;

bool is_crosshair(in vec2 position)
{
    const float w1 = CROSSHAIR_WIDTH;
    const float t1 = CROSSHAIR_THICKNESS;
    const float w2 = CROSSHAIR_WIDTH / window.size.x * window.size.y;
    const float t2 = CROSSHAIR_THICKNESS / window.size.x * window.size.y;
    const vec2 a1 = vec2(CROSSHAIR_CENTER - w2, CROSSHAIR_CENTER - t1);
    const vec2 b1 = vec2(CROSSHAIR_CENTER + w2, CROSSHAIR_CENTER + t1);
    const vec2 a2 = vec2(CROSSHAIR_CENTER - t2, CROSSHAIR_CENTER - w1);
    const vec2 b2 = vec2(CROSSHAIR_CENTER + t2, CROSSHAIR_CENTER + w1);
    return
        (position.x > a1.x && position.y > a1.y &&
            position.x < b1.x && position.y < b1.y) ||
        (position.x > a2.x && position.y > a2.y &&
            position.x < b2.x && position.y < b2.y);
}

bool is_block(in vec2 position)
{
    const float width = BLOCK_WIDTH / window.size.x * window.size.y;
    const float height = BLOCK_WIDTH;
    const vec2 a = vec2(BLOCK_LEFT, BLOCK_BOTTOM);
    const vec2 b = a + vec2(width, height);
    return 
        position.x > a.x && position.y > a.y &&
        position.x < b.x && position.y < b.y;
}

vec2 map_position_to_atlas_uv(in vec2 position)
{
    const float width = BLOCK_WIDTH / window.size.x * window.size.y;
    const float height = BLOCK_WIDTH;
    const float x = (position.x - BLOCK_LEFT) / width;
    const float y = (position.y - BLOCK_BOTTOM) / height;
    const float u = float(block.uv.x) * ATLAS_FACE_WIDTH / ATLAS_WIDTH;
    const float v = float(block.uv.y) * ATLAS_FACE_WIDTH / ATLAS_HEIGHT;
    const float c = u + x / ATLAS_X_FACES;
    const float d = v + y / ATLAS_Y_FACES;
    const float i = 1.0 / ATLAS_Y_FACES - d;
    return vec2(c, i);
}

void main()
{
    vec2 position = gl_FragCoord.xy / window.size;
    position.y = 1.0 - position.y;
    if (is_crosshair(position)) {
        color = vec4(1.0, 1.0, 1.0, 1.0);
    } else if (is_block(position)) {
        color = texture(atlas, map_position_to_atlas_uv(position));
    } else {
        discard;
    }
}