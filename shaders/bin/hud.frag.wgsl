struct type_u002e_UniformBuffer {
    Block: u32,
}

@group(2) @binding(0) 
var atlasTexture: texture_2d_array<f32>;
@group(2) @binding(1) 
var atlasSampler: sampler;
@group(3) @binding(0) 
var<uniform> UniformBuffer: type_u002e_UniformBuffer;
var<private> in_u002e_var_u002e_TEXCOORD0_1: vec2<f32>;
var<private> in_u002e_var_u002e_TEXCOORD1_1: u32;
var<private> out_u002e_var_u002e_SV_Target0_: vec4<f32>;

fn main_1() {
    var kLabels: array<vec2<u32>, 7>;
    var kLabelLengths: array<u32, 7>;
    var kFont8x8_: array<vec2<u32>, 96>;
    var phi_360_: bool;
    var phi_399_: bool;
    var phi_412_: u32;
    var phi_422_: bool;
    var phi_435_: u32;
    var phi_446_: bool;
    var phi_453_: vec4<f32>;

    kFont8x8_ = array<vec2<u32>, 96>(vec2<u32>(0u, 0u), vec2<u32>(406600728u, 1572888u), vec2<u32>(13878u, 0u), vec2<u32>(914306614u, 3552895u), vec2<u32>(503528972u, 794416u), vec2<u32>(406020864u, 6514188u), vec2<u32>(1847342620u, 7222075u), vec2<u32>(198150u, 0u), vec2<u32>(101059608u, 1575942u), vec2<u32>(404229126u, 396312u), vec2<u32>(4282148352u, 26172u), vec2<u32>(1057754112u, 3084u), vec2<u32>(0u, 101452800u), vec2<u32>(1056964608u, 0u), vec2<u32>(0u, 789504u), vec2<u32>(202911840u, 66310u), vec2<u32>(2071159614u, 4089711u), vec2<u32>(202116620u, 4131852u), vec2<u32>(472920862u, 4141830u), vec2<u32>(472920862u, 1979184u), vec2<u32>(859192376u, 7876735u), vec2<u32>(807338815u, 1979184u), vec2<u32>(520291868u, 1979187u), vec2<u32>(405812031u, 789516u), vec2<u32>(506671902u, 1979187u), vec2<u32>(1043542814u, 923696u), vec2<u32>(789504u, 789504u), vec2<u32>(789504u, 101452800u), vec2<u32>(50727960u, 1575942u), vec2<u32>(4128768u, 16128u), vec2<u32>(806882310u, 396312u), vec2<u32>(405811998u, 786444u), vec2<u32>(2071683902u, 1966971u), vec2<u32>(858988044u, 3355455u), vec2<u32>(1046898239u, 4154982u), vec2<u32>(50554428u, 3958275u), vec2<u32>(1717974559u, 2045542u), vec2<u32>(504776319u, 8341014u), vec2<u32>(504776319u, 984598u), vec2<u32>(50554428u, 8152691u), vec2<u32>(1060320051u, 3355443u), vec2<u32>(202116126u, 1969164u), vec2<u32>(808464504u, 1979187u), vec2<u32>(506881639u, 6776374u), vec2<u32>(101058063u, 8349254u), vec2<u32>(2139060067u, 6513515u), vec2<u32>(2070898531u, 6513523u), vec2<u32>(1667446300u, 1848931u), vec2<u32>(1046898239u, 984582u), vec2<u32>(858993438u, 3677755u), vec2<u32>(1046898239u, 6776374u), vec2<u32>(235352862u, 1979192u), vec2<u32>(202124607u, 1969164u), vec2<u32>(858993459u, 4141875u), vec2<u32>(858993459u, 794163u), vec2<u32>(1801675619u, 6518655u), vec2<u32>(473326435u, 6501916u), vec2<u32>(506671923u, 1969164u), vec2<u32>(405889919u, 8349260u), vec2<u32>(101058078u, 1967622u), vec2<u32>(403441155u, 4218928u), vec2<u32>(404232222u, 1972248u), vec2<u32>(1664490504u, 0u), vec2<u32>(0u, 4278190080u), vec2<u32>(1575948u, 0u), vec2<u32>(807272448u, 7222078u), vec2<u32>(1040582151u, 3892838u), vec2<u32>(857604096u, 1979139u), vec2<u32>(1043345464u, 7222067u), vec2<u32>(857604096u, 1966911u), vec2<u32>(252065308u, 984582u), vec2<u32>(862846976u, 523255347u), vec2<u32>(1849034247u, 6776422u), vec2<u32>(202244108u, 1969164u), vec2<u32>(808452144u, 506671920u), vec2<u32>(912655879u, 6764062u), vec2<u32>(202116110u, 1969164u), vec2<u32>(2134048768u, 6515583u), vec2<u32>(857669632u, 3355443u), vec2<u32>(857604096u, 1979187u), vec2<u32>(1715142656u, 252067430u), vec2<u32>(862846976u, 2016427571u), vec2<u32>(1849360384u, 984678u), vec2<u32>(54394880u, 2043934u), vec2<u32>(205392904u, 1584140u), vec2<u32>(858980352u, 7222067u), vec2<u32>(858980352u, 794163u), vec2<u32>(1801650176u, 3571583u), vec2<u32>(912457728u, 6501916u), vec2<u32>(858980352u, 523255347u), vec2<u32>(423559168u, 4138508u), vec2<u32>(118230072u, 3673100u), vec2<u32>(1579032u, 1579032u), vec2<u32>(940313607u, 461836u), vec2<u32>(15214u, 0u), vec2<u32>(0u, 0u));
    kLabelLengths = array<u32, 7>(5u, 5u, 4u, 6u, 1u, 3u, 1u);
    kLabels = array<vec2<u32>, 7>(vec2<u32>(1128352848u, 69u), vec2<u32>(1095062082u, 75u), vec2<u32>(1347245386u, 0u), vec2<u32>(1230131283u, 21582u), vec2<u32>(62u, 0u), vec2<u32>(5852230u, 0u), vec2<u32>(60u, 0u));
    let _e305 = in_u002e_var_u002e_TEXCOORD0_1;
    let _e306 = in_u002e_var_u002e_TEXCOORD1_1;
    switch bitcast<i32>(0u) {
        default: {
            let _e309 = UniformBuffer.Block;
            let _e313 = vec3<f32>(_e305.x, _e305.y, f32(_e309));
            let _e319 = textureSample(atlasTexture, atlasSampler, vec2<f32>(_e313.x, _e313.y), i32(_e313.z));
            let _e320 = (_e305 - vec2<f32>(0.5f, 0.5f));
            let _e321 = length(_e320);
            let _e322 = fwidth(_e321);
            phi_360_ = true;
            if (_e306 != 0u) {
                phi_360_ = (_e306 == 1u);
            }
            let _e326 = phi_360_;
            if _e326 {
                phi_453_ = vec4<f32>(1f, 1f, 1f, 1f);
                break;
            }
            if (_e306 == 2u) {
                if (_e319.w <= 0.001f) {
                    discard;
                }
                phi_453_ = _e319;
                break;
            }
            if (_e321 > 0.5f) {
                discard;
            }
            let _e336 = (_e306 - 3u);
            let _e338 = kLabels[_e336];
            let _e340 = kLabelLengths[_e336];
            switch bitcast<i32>(0u) {
                default: {
                    let _e344 = vec2<f32>(f32((_e340 * 8u)), 8f);
                    let _e348 = (((_e320 * 7.5f) * 8f) + (_e344 * 0.5f));
                    phi_399_ = true;
                    if !(any((_e348 < vec2<f32>(0f, 0f)))) {
                        phi_399_ = any((_e348 >= _e344));
                    }
                    let _e355 = phi_399_;
                    if _e355 {
                        phi_446_ = false;
                        break;
                    }
                    let _e356 = vec2<u32>(_e348);
                    let _e358 = (_e356.x / 8u);
                    let _e359 = (_e356 % vec2<u32>(8u, 8u));
                    if (_e358 < 4u) {
                        phi_412_ = _e338.x;
                    } else {
                        phi_412_ = _e338.y;
                    }
                    let _e364 = phi_412_;
                    let _e370 = ((_e364 >> bitcast<u32>((((_e358 & 3u) * 8u) & 31u))) & 255u);
                    phi_422_ = true;
                    if (_e370 >= 32u) {
                        phi_422_ = (_e370 > 127u);
                    }
                    let _e374 = phi_422_;
                    if _e374 {
                        phi_446_ = false;
                        break;
                    }
                    let _e377 = kFont8x8_[(_e370 - 32u)];
                    if (_e359.y < 4u) {
                        phi_435_ = _e377.x;
                    } else {
                        phi_435_ = _e377.y;
                    }
                    let _e383 = phi_435_;
                    phi_446_ = (((((_e383 >> bitcast<u32>((((_e359.y & 3u) * 8u) & 31u))) & 255u) >> bitcast<u32>((_e359.x & 31u))) & 1u) != 0u);
                    break;
                }
            }
            let _e397 = phi_446_;
            if _e397 {
                phi_453_ = vec4<f32>(0.86f, 0.88f, 0.92f, 0.95f);
                break;
            } else {
                phi_453_ = vec4<f32>(0.86f, 0.88f, 0.92f, (mix(0.12f, 0.75f, smoothstep((0.465f - _e322), 0.465f, _e321)) * (1f - smoothstep((0.5f - _e322), 0.5f, _e321))));
                break;
            }
        }
    }
    let _e402 = phi_453_;
    out_u002e_var_u002e_SV_Target0_ = _e402;
    return;
}

@fragment 
fn main(@location(0) in_u002e_var_u002e_TEXCOORD0_: vec2<f32>, @location(1) @interpolate(flat) in_u002e_var_u002e_TEXCOORD1_: u32) -> @location(0) vec4<f32> {
    in_u002e_var_u002e_TEXCOORD0_1 = in_u002e_var_u002e_TEXCOORD0_;
    in_u002e_var_u002e_TEXCOORD1_1 = in_u002e_var_u002e_TEXCOORD1_;
    main_1();
    let _e5 = out_u002e_var_u002e_SV_Target0_;
    return _e5;
}
