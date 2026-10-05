struct type_u002e_UniformBuffer {
    Proj: mat4x4<f32>,
}

struct type_u002e_UniformBuffer_1 {
    View: mat4x4<f32>,
}

struct type_u002e_UniformBuffer_2 {
    ChunkPosition: vec2<i32>,
}

struct VertexOutput {
    @builtin(position) member: vec4<f32>,
    @location(0) member_1: vec4<f32>,
    @location(1) member_2: vec2<f32>,
    @location(2) @interpolate(flat) member_3: u32,
    @location(3) member_4: f32,
}

@group(1) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
@group(1) @binding(1) 
var<uniform> UniformBuffer_1: type_u002e_UniformBuffer_1;
@group(1) @binding(2) 
var<uniform> UniformBuffer_2: type_u002e_UniformBuffer_2;
var<private> in_u002e_var_u002e_TEXCOORD0_1: u32;
var<private> global: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);
var<private> out_u002e_var_u002e_TEXCOORD0_: vec4<f32>;
var<private> out_u002e_var_u002e_TEXCOORD1_: vec2<f32>;
var<private> out_u002e_var_u002e_TEXCOORD2_: u32;
var<private> out_u002e_var_u002e_TEXCOORD3_: f32;

fn main_1() {
    var kAO: array<f32, 4>;

    kAO = array<f32, 4>(0.4f, 0.6f, 0.8f, 1f);
    let _e29 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e32 = UniformBuffer_2.ChunkPosition[0i];
    let _e35 = UniformBuffer_2.ChunkPosition[1i];
    let _e51 = (vec3<f32>(f32(((_e29 >> bitcast<u32>(10u)) & 31u)), f32(((_e29 >> bitcast<u32>(15u)) & 255u)), f32(((_e29 >> bitcast<u32>(23u)) & 31u))) + vec3<f32>(vec3<i32>(_e32, 0i, _e35)));
    let _e56 = vec4<f32>(_e51.x, _e51.y, _e51.z, vec4<f32>().w);
    let _e58 = UniformBuffer_1.View;
    let _e64 = (vec4<f32>(_e51.x, _e51.y, _e51.z, 1f) * transpose(_e58));
    let _e72 = UniformBuffer.Proj;
    let _e88 = kAO[((_e29 >> bitcast<u32>(8u)) & 3u)];
    global = (_e64 * transpose(_e72));
    out_u002e_var_u002e_TEXCOORD0_ = vec4<f32>(_e56.x, _e56.y, _e56.z, _e64.z);
    out_u002e_var_u002e_TEXCOORD1_ = vec2<f32>(f32(((_e29 >> bitcast<u32>(28u)) & 1u)), f32(((_e29 >> bitcast<u32>(29u)) & 1u)));
    out_u002e_var_u002e_TEXCOORD2_ = _e29;
    out_u002e_var_u002e_TEXCOORD3_ = _e88;
    return;
}

@vertex 
fn main(@location(0) in_u002e_var_u002e_TEXCOORD0_: u32) -> VertexOutput {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    main_1();
    let _e7 = global;
    let _e8 = out_u002e_var_u002e_TEXCOORD0_;
    let _e9 = out_u002e_var_u002e_TEXCOORD1_;
    let _e10 = out_u002e_var_u002e_TEXCOORD2_;
    let _e11 = out_u002e_var_u002e_TEXCOORD3_;
    return VertexOutput(_e7, _e8, _e9, _e10, _e11);
}
