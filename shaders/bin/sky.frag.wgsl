struct type_u002e_UniformBuffer {
    Sun: vec4<f32>,
    SkyTop: vec4<f32>,
    SkyHorizon: vec4<f32>,
    Ambient: vec4<f32>,
}

struct FragmentOutput {
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec4<f32>,
}

@group(3) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec3<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;
var<private> out_u002e_var_u002e_SV_Target1_: vec4<f32>;

fn main_1() {
    let _e11 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e13 = UniformBuffer.SkyTop;
    let _e16 = UniformBuffer.SkyHorizon;
    let _e19 = in_u002e_var_u002e_TEXCOORD0_1[1i];
    let _e26 = mix(_e16.xyz, _e13.xyz, vec3(((atan2(_e19, length(_e11.xz)) + 1.5707964f) * 0.31830987f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e26.x, _e26.y, _e26.z, 1f);
    out_u002e_var_u002e_SV_Target1_ = vec4<f32>(0f, 0f, 0f, 0f);
    return;
}

@fragment 
fn main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec3<f32>) -> FragmentOutput {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    main_1();
    let _e4 = out_u002e_var_u002e_SV_Target0_;
    let _e5 = out_u002e_var_u002e_SV_Target1_;
    return FragmentOutput(_e4, _e5);
}
