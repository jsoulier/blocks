struct type_u002e_UniformBuffer {
    Proj: mat4x4<f32>,
}

struct type_u002e_UniformBuffer_1 {
    View: mat4x4<f32>,
}

struct VertexOutput {
    @builtin(position) member: vec4<f32>,
    @location(0) member_1: vec3<f32>,
}

@group(1) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
@group(1) @binding(1) 
var<uniform> UniformBuffer_1: type_u002e_UniformBuffer_1;
var<private> global: u32;
var<private> global_1: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);
var<private> out_u002e_var_u002e_TEXCOORD0_: vec3<f32>;

fn main_1() {
    var kCubeIndices: array<u32, 36>;
    var kCubePositions: array<vec3<f32>, 8>;

    kCubePositions = array<vec3<f32>, 8>(vec3<f32>(-0.5f, -0.5f, -0.5f), vec3<f32>(0.5f, -0.5f, -0.5f), vec3<f32>(0.5f, 0.5f, -0.5f), vec3<f32>(-0.5f, 0.5f, -0.5f), vec3<f32>(-0.5f, -0.5f, 0.5f), vec3<f32>(0.5f, -0.5f, 0.5f), vec3<f32>(0.5f, 0.5f, 0.5f), vec3<f32>(-0.5f, 0.5f, 0.5f));
    kCubeIndices = array<u32, 36>(0u, 1u, 2u, 0u, 2u, 3u, 5u, 4u, 7u, 5u, 7u, 6u, 4u, 0u, 3u, 4u, 3u, 7u, 1u, 5u, 6u, 1u, 6u, 2u, 3u, 2u, 6u, 3u, 6u, 7u, 4u, 5u, 1u, 4u, 1u, 0u);
    let _e32 = global;
    let _e34 = UniformBuffer_1.View;
    let _e35 = transpose(_e34);
    let _e45 = mat4x4<f32>(vec4<f32>(_e35[0].x, _e35[0].y, _e35[0].z, 0f), _e35[1], _e35[2], _e35[3]);
    let _e55 = mat4x4<f32>(_e45[0], vec4<f32>(_e45[1].x, _e45[1].y, _e45[1].z, 0f), _e45[2], _e45[3]);
    let _e67 = kCubeIndices[_e32];
    let _e69 = kCubePositions[_e67];
    let _e71 = UniformBuffer.Proj;
    global_1 = ((vec4<f32>(_e69.x, _e69.y, _e69.z, 1f) * mat4x4<f32>(_e55[0], _e55[1], vec4<f32>(_e55[2].x, _e55[2].y, _e55[2].z, 0f), _e55[3])) * transpose(_e71));
    out_u002e_var_u002e_TEXCOORD0_ = _e69;
    return;
}

@vertex 
fn main(@builtin(vertex_index) param: u32) -> VertexOutput {
    global = param;
    main_1();
    let _e4 = global_1;
    let _e5 = out_u002e_var_u002e_TEXCOORD0_;
    return VertexOutput(_e4, _e5);
}
