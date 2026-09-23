// Drifting fog band. Translated from shaders/fog.frag,
// itself derived from Flame's crystal_ball example.

#import bevy_sprite::mesh2d_vertex_output::VertexOutput

struct FogParams {
    size: vec2<f32>,
    ground_pos: f32,
    ground_add: f32,
    fade: f32,
    time: f32,
    _padding: vec2<f32>,
};

@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<uniform> params: FogParams;

// iq's value hash, https://www.shadertoy.com/view/lsf3WH
fn hash(p_in: vec2<f32>) -> f32 {
    let p = 50.0 * fract(p_in * 0.3183099 + vec2<f32>(0.71, 0.113));
    return -1.0 + 2.0 * fract(p.x * p.y * (p.x + p.y));
}

fn noise(p: vec2<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    let u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    return mix(
        mix(hash(i + vec2<f32>(0.0, 0.0)), hash(i + vec2<f32>(1.0, 0.0)), u.x),
        mix(hash(i + vec2<f32>(0.0, 1.0)), hash(i + vec2<f32>(1.0, 1.0)), u.x),
        u.y,
    );
}

fn fractal_noise(uv_in: vec2<f32>) -> f32 {
    var uv = uv_in * 8.0;
    // Column-major, matching the GLSL mat2(1.6, 1.2, -1.2, 1.6).
    let m = mat2x2<f32>(vec2<f32>(1.6, 1.2), vec2<f32>(-1.2, 1.6));

    var f = 0.5 * noise(uv);
    uv = m * uv;
    f += 0.25 * noise(uv);
    uv = m * uv;
    f += 0.125 * noise(uv);
    uv = m * uv;
    f += 0.0625 * noise(uv);

    return 0.5 + 0.5 * f;
}

fn fog_layer(p: vec2<f32>, time_multiplier: f32) -> vec4<f32> {
    var uv = p * vec2<f32>(params.size.x / params.size.y, 1.0);

    let waterline = params.ground_pos;

    var tr = step(waterline - params.fade, uv.y);
    tr = tr * smoothstep(waterline - params.fade, waterline, uv.y);

    uv.y = uv.y - params.ground_add;
    uv.x = uv.x + params.time * time_multiplier;

    var f = fractal_noise(uv);
    f = f * tr;
    f = f * 0.65;
    f = pow(f, 1.8);

    return vec4<f32>(vec3<f32>(0.8, 0.4, 1.0) * f, f);
}

@fragment
fn fragment(mesh: VertexOutput) -> @location(0) vec4<f32> {
    // Two counter-drifting layers, as in the original `main`.
    return fog_layer(mesh.uv, 0.015) + fog_layer(mesh.uv, -0.08);
}
