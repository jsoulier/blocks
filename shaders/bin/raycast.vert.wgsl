struct type_u002e_UniformBuffer {
    Transform: mat4x4<f32>,
}

struct type_u002e_UniformBuffer_1 {
    BlockPosition: vec3<i32>,
}

@group(1) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
@group(1) @binding(1) 
var<uniform> UniformBuffer_1: type_u002e_UniformBuffer_1;
var<private> global: u32;
var<private> global_1: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);

fn main_1() {
    var kCubeIndices: array<u32, 36>;
    var kCubePositions: array<vec3<f32>, 8>;

    kCubePositions = array<vec3<f32>, 8>(vec3<f32>(-0.5f, -0.5f, -0.5f), vec3<f32>(0.5f, -0.5f, -0.5f), vec3<f32>(0.5f, 0.5f, -0.5f), vec3<f32>(-0.5f, 0.5f, -0.5f), vec3<f32>(-0.5f, -0.5f, 0.5f), vec3<f32>(0.5f, -0.5f, 0.5f), vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(-0.5f, 0.5f, 0.5f));
    kCubeIndices = array<u32, 36>(0u, 1u, 2u, 0u, 2u, 3u, 5u, 4u, 7u, 5u, 7u, 6u, 4u, 0u, 3u, 4u, 3u, 7u, 1u, 5u, 6u, 1u, 6u, 2u, 3u, 2u, 6u, 3u, 6u, 7u, 4u, 5u, 1u, 4u, 1u, 0u);
    let _e31 = global;
    let _e33 = kCubeIndices[_e31];
    let _e35 = kCubePositions[_e33];
    let _e38 = UniformBuffer_1.BlockPosition;
    let _e41 = ((_e35 * 1.025f) + (vec3<f32>(_e38) + vec3<f32>(0.5f, 0.5f, 0.5f)));
    let _e43 = UniformBuffer.Transform;
    global_1 = (vec4<f32>(_e41.x, _e41.y, _e41.z, 1f) * transpose(_e43));
    return;
}

@vertex 
fn main(@builtin(vertex_index) param: u32) -> @builtin(position) vec4<f32> {
    global = param;
    main_1();
    let _e3 = global_1;
    return _e3;
}
