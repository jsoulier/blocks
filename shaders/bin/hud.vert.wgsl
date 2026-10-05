struct type_u002e_UniformBuffer {
    Viewport: vec2<i32>,
    Touch: i32,
    Padding: i32,
    Safe: vec4<i32>,
}

struct VertexOutput {
    @builtin(position) member: vec4<f32>,
    @location(0) member_1: vec2<f32>,
    @location(1) @interpolate(flat) member_2: u32,
}

@group(1) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
var<private> global: u32;
var<private> global_1: u32;
var<private> global_2: vec4<f32> = vec4<f32>(0f, 0f, 0f, 1f);
var<private> out_u002e_var_u002e_TEXCOORD0_: vec2<f32>;
var<private> out_u002e_var_u002e_TEXCOORD1_: u32;

fn main_1() {
    var kButtons: array<vec2<f32>, 7>;
    var kPositions: array<vec2<f32>, 6>;
    var phi_93_: bool;
    var phi_128_: bool;
    var phi_142_: f32;
    var phi_152_: vec2<f32>;
    var phi_153_: vec2<f32>;
    var phi_154_: vec2<f32>;
    var phi_155_: vec2<f32>;
    var phi_156_: vec2<f32>;
    var phi_157_: vec2<f32>;

    kPositions = array<vec2<f32>, 6>(vec2<f32>(0f, 0f), vec2<f32>(1f, 0f), vec2<f32>(0f, 1f), vec2<f32>(0f, 1f), vec2<f32>(1f, 0f), vec2<f32>(1f, 1f));
    kButtons = array<vec2<f32>, 7>(vec2<f32>(100f, 100f), vec2<f32>(210f, 100f), vec2<f32>(100f, 210f), vec2<f32>(210f, 210f), vec2<f32>(100f, 320f), vec2<f32>(100f, 210f), vec2<f32>(100f, 320f));
    let _e45 = global;
    let _e46 = global_1;
    let _e48 = UniformBuffer.Viewport;
    let _e49 = vec2<f32>(_e48);
    let _e51 = UniformBuffer.Safe;
    let _e52 = vec4<f32>(_e51);
    let _e54 = (_e49.x * 0.00078125f);
    let _e56 = (_e49.y * 0.0013888889f);
    let _e58 = select(_e56, _e54, (_e54 > _e56));
    phi_93_ = true;
    if (_e46 != 0u) {
        phi_93_ = (_e46 == 1u);
    }
    let _e62 = phi_93_;
    if _e62 {
        let _e63 = (_e49 * 0.5f);
        let _e67 = (select(vec2<f32>(2f, 8f), vec2<f32>(8f, 2f), vec2((_e46 == 0u))) * _e58);
        phi_156_ = (_e63 + _e67);
        phi_157_ = (_e63 - _e67);
    } else {
        if (_e46 == 2u) {
            let _e74 = (_e52.xy + vec2((10f * _e58)));
            phi_154_ = (_e74 + vec2((50f * _e58)));
            phi_155_ = _e74;
        } else {
            let _e79 = UniformBuffer.Touch;
            if (_e79 == 0i) {
                phi_152_ = vec2<f32>(0f, 0f);
                phi_153_ = vec2<f32>(0f, 0f);
            } else {
                let _e81 = (_e46 - 3u);
                let _e83 = kButtons[_e81];
                phi_128_ = true;
                if (_e81 != 5u) {
                    phi_128_ = (_e81 == 6u);
                }
                let _e87 = phi_128_;
                if _e87 {
                    phi_142_ = (_e52.x + (_e83.x * _e58));
                } else {
                    phi_142_ = ((_e52.x + _e52.z) - (_e83.x * _e58));
                }
                let _e99 = phi_142_;
                let _e104 = vec2<f32>(_e99, (_e52.y + (_e83.y * _e58)));
                let _e106 = vec2((45f * _e58));
                phi_152_ = (_e104 + _e106);
                phi_153_ = (_e104 - _e106);
            }
            let _e110 = phi_152_;
            let _e112 = phi_153_;
            phi_154_ = _e110;
            phi_155_ = _e112;
        }
        let _e114 = phi_154_;
        let _e116 = phi_155_;
        phi_156_ = _e114;
        phi_157_ = _e116;
    }
    let _e118 = phi_156_;
    let _e120 = phi_157_;
    let _e122 = kPositions[_e45];
    let _e126 = (((mix(_e120, _e118, _e122) / _e49) * 2f) - vec2<f32>(1f, 1f));
    let _e132 = kPositions[_e45][0i];
    let _e135 = kPositions[_e45][1i];
    global_2 = vec4<f32>(_e126.x, _e126.y, 0f, 1f);
    out_u002e_var_u002e_TEXCOORD0_ = vec2<f32>(_e132, (1f - _e135));
    out_u002e_var_u002e_TEXCOORD1_ = _e46;
    return;
}

@vertex 
fn main(@builtin(vertex_index) param: u32, @builtin(instance_index) param_1: u32) -> VertexOutput {
    global = param;
    global_1 = param_1;
    main_1();
    let _e7 = global_2;
    let _e8 = out_u002e_var_u002e_TEXCOORD0_;
    let _e9 = out_u002e_var_u002e_TEXCOORD1_;
    return VertexOutput(_e7, _e8, _e9);
}
