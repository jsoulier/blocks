var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn main_1() {
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(1f, 1f, 1f, 0.1f);
    return;
}

@fragment 
fn main() -> @location(0) vec4<f32> {
    main_1();
    let _e1 = out_u002e_var_u002e_SV_Target0_;
    return _e1;
}
