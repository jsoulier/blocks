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
    @location(3) @interpolate(linear) member_4: vec2<f32>,
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
var<private> out_u002e_var_u002e_TEXCOORD3_: vec2<f32>;

fn main_1() {
    let _e23 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e26 = UniformBuffer_2.ChunkPosition[0i];
    let _e29 = UniformBuffer_2.ChunkPosition[1i];
    let _e45 = (vec3<f32>(f32(((_e23 >> bitcast<u32>(10u)) & 31u)), f32(((_e23 >> bitcast<u32>(15u)) & 255u)), f32(((_e23 >> bitcast<u32>(23u)) & 31u))) + vec3<f32>(vec3<i32>(_e26, 0i, _e29)));
    let _e50 = vec4<f32>(_e45.x, _e45.y, _e45.z, vec4<f32>().w);
    let _e52 = UniformBuffer_1.View;
    let _e58 = (vec4<f32>(_e45.x, _e45.y, _e45.z, 1f) * transpose(_e52));
    let _e66 = UniformBuffer.Proj;
    let _e68 = (_e58 * transpose(_e66));
    let _e83 = (((_e68.xy / vec2(_e68.w)) * 0.5f) + vec2<f32>(0.5f, 0.5f));
    global = _e68;
    out_u002e_var_u002e_TEXCOORD0_ = vec4<f32>(_e50.x, _e50.y, _e50.z, _e58.z);
    out_u002e_var_u002e_TEXCOORD1_ = vec2<f32>(f32(((_e23 >> bitcast<u32>(28u)) & 1u)), f32(((_e23 >> bitcast<u32>(29u)) & 1u)));
    out_u002e_var_u002e_TEXCOORD2_ = _e23;
    out_u002e_var_u002e_TEXCOORD3_ = vec2<f32>(_e83.x, (1f - _e83.y));
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
