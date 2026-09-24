// Expanding ring shown when a heart is collected.
// Translated from shaders/shockwave.frag.
//
// The GLSL derived its UV from FlutterFragCoord()/uSize; a Material2d already
// hands us that as mesh.uv, with the same top-left origin, so the division is
// dropped and `size` is kept only for the aspect correction.

#import bevy_sprite::mesh2d_vertex_output::VertexOutput

struct ShockwaveParams {
    size: vec2<f32>,
    center: vec2<f32>,
    time: f32,
    progress: f32,
    max_radius: f32,
    width: f32,
};

@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<uniform> params: ShockwaveParams;

@fragment
fn fragment(mesh: VertexOutput) -> @location(0) vec4<f32> {
    var p = mesh.uv - params.center;
    p.x = p.x * (params.size.x / params.size.y);
    let dist = length(p);

    // Small ramp so radius 0 is never a special case at progress 0.
    let ramp = smoothstep(0.02, 1.0, params.progress);
    let radius = params.max_radius * ramp;

    let ripple = (params.max_radius * 0.01)
        * sin(40.0 * dist - 6.0 * params.progress + params.time * 6.0);

    // The ring thins slightly as it expands.
    let w = max(0.001, params.width * (1.0 - params.progress * 0.8));

    let d = abs(dist - radius + ripple);
    let ring = 1.0 - smoothstep(0.5 * w, 1.5 * w, d);

    let fade = ramp * (1.0 - params.progress);
    let alpha = clamp(ring * fade, 0.0, 1.0);

    let color = vec3<f32>(1.0, 0.95, 0.7);
    let mask = smoothstep(0.001, 0.004, alpha);
    // The GLSL emitted premultiplied colour because Flame drew it with
    // BlendMode.plus; this port uses straight alpha blending, so the colour is
    // left unmultiplied and the blend does the work.
    return vec4<f32>(color, alpha * mask);
}
