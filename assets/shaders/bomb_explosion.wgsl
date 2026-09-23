// Cartoon explosion. Translated from shaders/bomb_explosion.frag
// (inspired by Krapas, https://www.shadertoy.com/view/X3dGz2).
//
// The GLSL indexed palette arrays with a runtime int through an if/else chain
// to stay within Flutter's runtime-effect restrictions; WGSL indexes arrays
// directly, so the chains collapse into a clamped lookup.

#import bevy_sprite::mesh2d_vertex_output::VertexOutput

struct ExplosionParams {
    size: vec2<f32>,
    time: f32,
    progress: f32,
};

@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<uniform> params: ExplosionParams;

fn n_rand3(p: vec3<f32>) -> vec3<f32> {
    let r = fract(sin(vec3<f32>(
        dot(p, vec3<f32>(127.1, 311.7, 371.8)),
        dot(p, vec3<f32>(269.5, 183.3, 456.1)),
        dot(p, vec3<f32>(352.5, 207.3, 198.67)),
    )) * 43758.5453) * 2.0 - 1.0;
    return normalize(vec3<f32>(r.x / cos(r.x), r.y / cos(r.y), r.z / cos(r.z)));
}

fn noise3(p: vec3<f32>) -> f32 {
    let fv = fract(p);
    let nv = floor(p);
    let u = fv * fv * fv * (fv * (fv * 6.0 - 15.0) + 10.0);

    let c000 = dot(n_rand3(nv + vec3<f32>(0.0, 0.0, 0.0)), fv - vec3<f32>(0.0, 0.0, 0.0));
    let c100 = dot(n_rand3(nv + vec3<f32>(1.0, 0.0, 0.0)), fv - vec3<f32>(1.0, 0.0, 0.0));
    let c010 = dot(n_rand3(nv + vec3<f32>(0.0, 1.0, 0.0)), fv - vec3<f32>(0.0, 1.0, 0.0));
    let c110 = dot(n_rand3(nv + vec3<f32>(1.0, 1.0, 0.0)), fv - vec3<f32>(1.0, 1.0, 0.0));
    let c001 = dot(n_rand3(nv + vec3<f32>(0.0, 0.0, 1.0)), fv - vec3<f32>(0.0, 0.0, 1.0));
    let c101 = dot(n_rand3(nv + vec3<f32>(1.0, 0.0, 1.0)), fv - vec3<f32>(1.0, 0.0, 1.0));
    let c011 = dot(n_rand3(nv + vec3<f32>(0.0, 1.0, 1.0)), fv - vec3<f32>(0.0, 1.0, 1.0));
    let c111 = dot(n_rand3(nv + vec3<f32>(1.0, 1.0, 1.0)), fv - vec3<f32>(1.0, 1.0, 1.0));

    return mix(
        mix(mix(c000, c100, u.x), mix(c010, c110, u.x), u.y),
        mix(mix(c001, c101, u.x), mix(c011, c111, u.x), u.y),
        u.z,
    );
}

fn worley(s: vec3<f32>) -> f32 {
    let si = floor(s);
    let sf = fract(s);
    var m_dist = 1.0;
    for (var y = -1; y <= 1; y = y + 1) {
        for (var x = -1; x <= 1; x = x + 1) {
            for (var z = -1; z <= 1; z = z + 1) {
                let neighbor = vec3<f32>(f32(x), f32(y), f32(z));
                var point = fract(n_rand3(si + neighbor));
                point = 0.5 + 0.5 * sin(params.time + 6.2831 * point);
                m_dist = min(m_dist, length(neighbor + point - sf));
            }
        }
    }
    return m_dist;
}

fn boom(p: vec2<f32>, t: f32) -> f32 {
    let shape = 1.0
        - pow(distance(vec3<f32>(p, 0.0), vec3<f32>(0.0)), 2.0) / (t * 12.0)
        - t * 2.0;
    let distortion = noise3(vec3<f32>(p * 0.5, params.time * 0.5));
    let bubbles = 0.5 - pow(worley(vec3<f32>(p * 1.2, params.time * 2.0)), 3.0);
    let bw = 0.5;
    return shape + (bw * bubbles + (1.0 - bw) * distortion);
}

fn smoke(p: vec2<f32>, t: f32) -> f32 {
    let drift = vec2<f32>(0.0, 2.0) * pow(t / 1.45, 2.0) * 1.5;
    let shape = 1.0
        - pow(distance(vec3<f32>(p + drift, 0.0), vec3<f32>(0.0)), 2.0) / (t * 16.0)
        - pow(t * 1.5, 0.5);
    let distortion = noise3(vec3<f32>(p * 1.5 + drift, params.time * 0.1));
    let bubble_drift = vec2<f32>(0.0, 2.0) * pow(t / 1.65, 2.0) * 1.5;
    let bubbles = 0.5
        - pow(worley(vec3<f32>(p / pow(t, 0.35) + bubble_drift, params.time * 0.1)), 2.0);
    let bw = 0.75;
    return shape + (bw * bubbles + (1.0 - bw) * distortion);
}

fn posterize(v: f32, n: f32) -> f32 {
    return floor(v * n) / (n - 1.0);
}

@fragment
fn fragment(mesh: VertexOutput) -> @location(0) vec4<f32> {
    let boom_pal = array<vec3<f32>, 4>(
        vec3<f32>(0.2, 0.15, 0.3),
        vec3<f32>(0.9, 0.15, 0.05),
        vec3<f32>(0.9, 0.5, 0.1),
        vec3<f32>(0.95, 0.95, 0.35),
    );
    let smoke_pal = array<vec3<f32>, 3>(
        vec3<f32>(0.2, 0.15, 0.3),
        vec3<f32>(0.35, 0.3, 0.45),
        vec3<f32>(0.5, 0.45, 0.6),
    );

    let center = vec2<f32>(0.5, 0.4);
    var p = mesh.uv - center;
    p.x = p.x * (params.size.x / params.size.y);
    p = p * 7.0;
    let t = max(params.progress, 0.01);

    let bpl = 4.0;
    let spl = 3.0;

    let boom_val = boom(p, t);
    let boom_a = step(0.0, boom_val);
    let boom_idx = clamp(i32(posterize(boom_val, bpl) * bpl), 0, i32(bpl) - 1);
    let boom_col = boom_pal[boom_idx] - vec3<f32>(1.0 - boom_a);

    let smoke_val = smoke(p, t);
    let smoke_a = step(0.0, smoke_val);
    let smoke_idx = clamp(i32(posterize(smoke_val, spl) * spl), 0, i32(spl) - 1);
    let smoke_col = smoke_pal[smoke_idx] - vec3<f32>(1.0 - smoke_a);

    let bw = step(smoke_val * 1.25, boom_val);
    let color = bw * boom_col + (1.0 - bw) * smoke_col;
    var alpha = bw * boom_a + (1.0 - bw) * smoke_a;

    // Soft fade at the bottom so the blast sits on the ground.
    //
    // The GLSL wrote `smoothstep(0.18, 0.0, p.y/7.0)` — a reversed smoothstep,
    // which GLSL tolerates but WGSL leaves undefined when `low >= high` (on
    // Metal it returns 0, erasing the whole effect). Written the defined way.
    alpha = alpha * (1.0 - smoothstep(0.0, 0.18, p.y / 7.0));

    return vec4<f32>(color, alpha);
}
