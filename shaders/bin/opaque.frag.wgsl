struct Light {
    Position: vec3<i32>,
    Color: u32,
}

struct Material {
    Indices: array<u32, 6>,
    Padding0_: u32,
    Padding1_: u32,
    _Light: Light,
    IsOpaque: u32,
    IsSolid: u32,
    IsSprite: u32,
    UseAO: u32,
}

struct type_u002e_StructuredBuffer_u002e_Material {
    member: array<Material>,
}

struct type_u002e_StructuredBuffer_u002e_Light {
    member: array<Light>,
}

struct type_u002e_UniformBuffer {
    LightCount: i32,
}

struct type_u002e_UniformBuffer_1 {
    PlayerPosition: vec3<f32>,
}

struct type_u002e_UniformBuffer_2 {
    Sun: vec4<f32>,
    SkyTop: vec4<f32>,
    SkyHorizon: vec4<f32>,
    Ambient: vec4<f32>,
}

struct FragmentOutput {
    @location(0) member: vec4<f32>,
    @location(1) member_1: vec4<f32>,
}

@group(2) @binding(0) 
var atlasTexture: texture_2d_array<f32>;
@group(2) @binding(1) 
var atlasSampler: sampler;
@group(2) @binding(2) 
var<storage> materialBuffer: type_u002e_StructuredBuffer_u002e_Material;
@group(2) @binding(3) 
var<storage> lightBuffer: type_u002e_StructuredBuffer_u002e_Light;
@group(3) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
@group(3) @binding(1) 
var<uniform> UniformBuffer_1: type_u002e_UniformBuffer_1;
@group(3) @binding(2) 
var<uniform> UniformBuffer_2: type_u002e_UniformBuffer_2;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec4<f32>;
var<private> in_u002e_var_u002e_TEXCOORD1_1: vec2<f32>;
var<private> in_u002e_var_u002e_TEXCOORD2_1: u32;
var<private> in_u002e_var_u002e_TEXCOORD3_1: f32;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;
var<private> out_u002e_var_u002e_SV_Target1_: vec4<f32>;

fn main_1() {
    var kNormals: array<vec3<f32>, 6>;
    var phi_132_: vec3<f32>;
    var phi_135_: u32;
    var phi_133_: vec3<f32>;
    var phi_188_: f32;
    var local: vec3<f32>;
    var phi_193_: vec3<f32>;
    var local_1: vec3<f32>;

    kNormals = array<vec3<f32>, 6>(vec3<f32>(0f, 0f, 1f), vec3<f32>(0f, 0f, -1f), vec3<f32>(1f, 0f, 0f), vec3<f32>(-1f, 0f, 0f), vec3<f32>(0f, 1f, 0f), vec3<f32>(0f, -1f, 0f));
    let _e52 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e53 = in_u002e_var_u002e_TEXCOORD1_1;
    let _e54 = in_u002e_var_u002e_TEXCOORD2_1;
    let _e55 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e58 = ((_e54 >> bitcast<u32>(3u)) & 31u);
    let _e59 = (_e54 & 7u);
    let _e64 = materialBuffer.member[_e58].Indices[_e59];
    let _e68 = vec3<f32>(_e53.x, _e53.y, f32(_e64));
    let _e74 = textureSample(atlasTexture, atlasSampler, vec2<f32>(_e68.x, _e68.y), i32(_e68.z));
    if (_e74.w < 0.001f) {
        discard;
    }
    let _e79 = kNormals[_e59];
    let _e81 = UniformBuffer.LightCount;
    let _e83 = _e52.xyz;
    let _e85 = UniformBuffer_2.Sun;
    phi_132_ = vec3<f32>(0f, 0f, 0f);
    phi_135_ = 0u;
    loop {
        let _e89 = phi_132_;
        let _e91 = phi_135_;
        local = _e89;
        local_1 = _e89;
        if (_e91 < bitcast<u32>(_e81)) {
            let _e95 = lightBuffer.member[_e91];
            let _e101 = f32(((_e95.Color & 4278190080u) >> bitcast<u32>(24u)));
            let _e104 = ((vec3<f32>(_e95.Position) + vec3<f32>(0.5f, 1f, 0.5f)) - _e83);
            let _e105 = length(_e104);
            if (_e105 >= _e101) {
                phi_133_ = _e89;
                continue;
            }
            phi_133_ = (_e89 + ((vec3<f32>((f32((_e95.Color & 255u)) * 0.003921569f), (f32(((_e95.Color & 65280u) >> bitcast<u32>(8u))) * 0.003921569f), (f32(((_e95.Color & 16711680u) >> bitcast<u32>(16u))) * 0.003921569f)) * mix(0.25f, 1f, clamp(dot(_e79, (_e104 / vec3(_e105))), 0f, 1f))) * (1f - smoothstep(0f, 1f, clamp((_e105 / _e101), 0f, 1f)))));
            continue;
        } else {
            break;
        }
        continuing {
            let _e134 = phi_133_;
            phi_132_ = _e134;
            phi_135_ = (_e91 + 1u);
        }
    }
    let _e193 = local_1;
    phi_193_ = _e193;
    if (_e85.w > 0f) {
        if (_e58 == 8u) {
            phi_188_ = 1f;
        } else {
            phi_188_ = clamp(-(dot(_e79, _e85.xyz)), 0f, 1f);
        }
        let _e142 = phi_188_;
        let _e147 = local;
        phi_193_ = (_e147 + vec3(((0.55f * _e85.w) * _e142)));
    }
    let _e150 = phi_193_;
    let _e152 = UniformBuffer_2.Ambient;
    let _e155 = UniformBuffer_1.PlayerPosition;
    let _e156 = (_e83 - _e155);
    let _e158 = UniformBuffer_2.SkyTop;
    let _e161 = UniformBuffer_2.SkyHorizon;
    let _e181 = mix((_e74.xyz * (_e150 + (_e152.xyz * _e55))), mix(_e161.xyz, _e158.xyz, vec3(((atan2(_e156.y, length(_e156.xz)) + 1.5707964f) * 0.31830987f))), vec3(min(pow((distance(_e52.xz, _e155.xz) * 0.004f), 2.5f), 1f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e181.x, _e181.y, _e181.z, 1f);
    out_u002e_var_u002e_SV_Target1_ = _e52;
    return;
}

@fragment 
fn main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec4<f32>, @location(1) in_u002e_var_u002e_TEXCOORD1_: vec2<f32>, @location(2) @interpolate(flat) in_u002e_var_u002e_TEXCOORD2_: u32, @location(3) in_u002e_var_u002e_TEXCOORD3_: f32) -> FragmentOutput {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD1_1 = in_u002e_var_u002e_TEXCOORD1_;
    in_u002e_var_u002e_TEXCOORD2_1 = in_u002e_var_u002e_TEXCOORD2_;
    in_u002e_var_u002e_TEXCOORD3_1 = in_u002e_var_u002e_TEXCOORD3_;
    main_1();
    let _e10 = out_u002e_var_u002e_SV_Target0_;
    let _e11 = out_u002e_var_u002e_SV_Target1_;
    return FragmentOutput(_e10, _e11);
}
