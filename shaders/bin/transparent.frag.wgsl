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

@group(2) @binding(0) 
var atlasTexture: texture_2d_array<f32>;
@group(2) @binding(1) 
var atlasSampler: sampler;
@group(2) @binding(2) 
var positionTexture: texture_2d<f32>;
@group(2) @binding(3) 
var positionSampler: sampler;
@group(2) @binding(4) 
var<storage> materialBuffer: type_u002e_StructuredBuffer_u002e_Material;
@group(2) @binding(5) 
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
var<private> in_u002e_var_u002e_TEXCOORD3_1: vec2<f32>;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn main_1() {
    var kNormals: array<vec3<f32>, 6>;
    var phi_133_: vec3<f32>;
    var phi_136_: u32;
    var phi_134_: vec3<f32>;
    var phi_189_: f32;
    var local: vec3<f32>;
    var phi_194_: vec3<f32>;
    var phi_241_: f32;
    var phi_242_: f32;
    var local_1: vec3<f32>;

    kNormals = array<vec3<f32>, 6>(vec3<f32>(0f, 0f, 1f), vec3<f32>(0f, 0f, -1f), vec3<f32>(1f, 0f, 0f), vec3<f32>(-1f, 0f, 0f), vec3<f32>(0f, 1f, 0f), vec3<f32>(0f, -1f, 0f));
    let _e54 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e55 = in_u002e_var_u002e_TEXCOORD1_1;
    let _e56 = in_u002e_var_u002e_TEXCOORD2_1;
    let _e57 = in_u002e_var_u002e_TEXCOORD3_1;
    let _e60 = ((_e56 >> bitcast<u32>(3u)) & 31u);
    let _e61 = (_e56 & 7u);
    let _e66 = materialBuffer.member[_e60].Indices[_e61];
    let _e70 = vec3<f32>(_e55.x, _e55.y, f32(_e66));
    let _e76 = textureSample(atlasTexture, atlasSampler, vec2<f32>(_e70.x, _e70.y), i32(_e70.z));
    let _e79 = kNormals[_e61];
    let _e81 = UniformBuffer.LightCount;
    let _e83 = _e54.xyz;
    let _e85 = UniformBuffer_2.Sun;
    phi_133_ = vec3<f32>(0f, 0f, 0f);
    phi_136_ = 0u;
    loop {
        let _e89 = phi_133_;
        let _e91 = phi_136_;
        local = _e89;
        local_1 = _e89;
        if (_e91 < bitcast<u32>(_e81)) {
            let _e95 = lightBuffer.member[_e91];
            let _e101 = f32(((_e95.Color & 4278190080u) >> bitcast<u32>(24u)));
            let _e104 = ((vec3<f32>(_e95.Position) + vec3<f32>(0.5f, 1f, 0.5f)) - _e83);
            let _e105 = length(_e104);
            if (_e105 >= _e101) {
                phi_134_ = _e89;
                continue;
            }
            phi_134_ = (_e89 + ((vec3<f32>((f32((_e95.Color & 255u)) * 0.003921569f), (f32(((_e95.Color & 65280u) >> bitcast<u32>(8u))) * 0.003921569f), (f32(((_e95.Color & 16711680u) >> bitcast<u32>(16u))) * 0.003921569f)) * mix(0.25f, 1f, clamp(dot(_e79, (_e104 / vec3(_e105))), 0f, 1f))) * (1f - smoothstep(0f, 1f, clamp((_e105 / _e101), 0f, 1f)))));
            continue;
        } else {
            break;
        }
        continuing {
            let _e134 = phi_134_;
            phi_133_ = _e134;
            phi_136_ = (_e91 + 1u);
        }
    }
    let _e209 = local_1;
    phi_194_ = _e209;
    if (_e85.w > 0f) {
        if (_e60 == 8u) {
            phi_189_ = 1f;
        } else {
            phi_189_ = clamp(-(dot(_e79, _e85.xyz)), 0f, 1f);
        }
        let _e142 = phi_189_;
        let _e147 = local;
        phi_194_ = (_e147 + vec3(((0.55f * _e85.w) * _e142)));
    }
    let _e150 = phi_194_;
    let _e152 = UniformBuffer_2.Ambient;
    let _e155 = UniformBuffer_1.PlayerPosition;
    let _e156 = (_e83 - _e155);
    let _e158 = UniformBuffer_2.SkyTop;
    let _e161 = UniformBuffer_2.SkyHorizon;
    phi_242_ = _e76.w;
    if (_e60 == 14u) {
        let _e179 = textureSampleLevel(positionTexture, positionSampler, _e57, 0f);
        phi_241_ = _e76.w;
        if (_e179.w != 0f) {
            let _e183 = in_u002e_var_u002e_TEXCOORD0_1[1i];
            phi_241_ = clamp((_e76.w + (max(0f, (_e183 - _e179.y)) * 0.1f)), 0f, 1f);
        }
        let _e191 = phi_241_;
        phi_242_ = _e191;
    }
    let _e193 = phi_242_;
    let _e197 = mix((_e76.xyz * (_e150 + _e152.xyz)), mix(_e161.xyz, _e158.xyz, vec3(((atan2(_e156.y, length(_e156.xz)) + 1.5707964f) * 0.31830987f))), vec3(min(pow((distance(_e54.xz, _e155.xz) * 0.004f), 2.5f), 1f)));
    out_u002e_var_u002e_SV_Target0_ = vec4<f32>(_e197.x, _e197.y, _e197.z, _e193);
    return;
}

@fragment 
fn main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec4<f32>, @location(1) in_u002e_var_u002e_TEXCOORD1_: vec2<f32>, @location(2) @interpolate(flat) in_u002e_var_u002e_TEXCOORD2_: u32, @location(3) @interpolate(linear) in_u002e_var_u002e_TEXCOORD3_: vec2<f32>) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD1_1 = in_u002e_var_u002e_TEXCOORD1_;
    in_u002e_var_u002e_TEXCOORD2_1 = in_u002e_var_u002e_TEXCOORD2_;
    in_u002e_var_u002e_TEXCOORD3_1 = in_u002e_var_u002e_TEXCOORD3_;
    main_1();
    let _e9 = out_u002e_var_u002e_SV_Target0_;
    return _e9;
}
