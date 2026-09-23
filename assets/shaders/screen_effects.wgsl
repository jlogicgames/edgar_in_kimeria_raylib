// Full-screen post process combining the two screen-space effects the Dart
// applied to the whole scene: the ripple displacement (`ripple.frag`, driven by
// `RippleDecorator`) and the chromatic-aberration glitch (`chroma_glitch.frag`,
// driven by `ChromaGlitchPostProcess`).
//
// Both sampled the finished frame and wrote a distorted copy, so they are one
// pass here: the ripple decides *where* to sample, the glitch decides how the
// three channels are offset from that point. Running them as two passes would
// mean a second full-screen copy for no visual difference.

#import bevy_core_pipeline::fullscreen_vertex_shader::FullscreenVertexOutput

struct ScreenEffects {
    // --- ripple (ripple.frag) ---
    ripple_center: vec2<f32>,
    ripple_progress: f32,
    ripple_max_radius: f32,
    ripple_strength: f32,
    ripple_frequency: f32,
    ripple_decay: f32,
    /// 0 or 1: whether a ripple is live this frame.
    ripple_active: f32,

    // --- chromatic glitch (chroma_glitch.frag) ---
    chroma_intensity: f32,
    chroma_shift: f32,

    time: f32,
    /// Viewport width / height, for the ripple's aspect correction.
    aspect: f32,
};

@group(0) @binding(0) var screen_texture: texture_2d<f32>;
@group(0) @binding(1) var texture_sampler: sampler;
@group(0) @binding(2) var<uniform> settings: ScreenEffects;

/// Radial sinusoidal displacement centred on a ring that expands with progress.
fn ripple_offset(uv: vec2<f32>) -> vec2<f32> {
    if settings.ripple_active < 0.5 {
        return vec2<f32>(0.0);
    }

    var p = uv - settings.ripple_center;
    p.x = p.x * settings.aspect;
    let dist = length(p);

    let radius = settings.ripple_max_radius * settings.ripple_progress;
    let d = dist - radius;

    let ripple = sin(d * settings.ripple_frequency - settings.time * 6.0);
    // Envelope: strongest at the ring, decaying either side of it.
    let env = exp(-abs(d) * settings.ripple_decay);
    let disp = settings.ripple_strength * ripple * env;

    var dir = vec2<f32>(0.0);
    if dist > 0.0001 {
        dir = p / dist;
    }
    var offset = dir * disp;
    // Undo the aspect correction on the way back into UV space.
    offset.x = offset.x / settings.aspect;
    return offset;
}

@fragment
fn fragment(in: FullscreenVertexOutput) -> @location(0) vec4<f32> {
    let uv = in.uv + ripple_offset(in.uv);

    if settings.chroma_intensity <= 0.0 {
        return textureSample(screen_texture, texture_sampler, clamp(uv, vec2(0.0), vec2(1.0)));
    }

    let red_shift = settings.chroma_intensity * settings.chroma_shift;
    let blue_shift = -red_shift;
    // Horizontal shake, proportional to the shift as in the original.
    let shake_x = sin(settings.time * 15.0) * (settings.chroma_shift * 0.25)
        * settings.chroma_intensity;
    let shaken = uv + vec2<f32>(shake_x, 0.0);

    let red_uv = clamp(shaken + vec2<f32>(red_shift, 0.0), vec2(0.0), vec2(1.0));
    let green_uv = clamp(shaken, vec2(0.0), vec2(1.0));
    let blue_uv = clamp(shaken + vec2<f32>(blue_shift, 0.0), vec2(0.0), vec2(1.0));

    let red = textureSample(screen_texture, texture_sampler, red_uv).r;
    let green_sample = textureSample(screen_texture, texture_sampler, green_uv);
    let blue = textureSample(screen_texture, texture_sampler, blue_uv).b;

    return vec4<f32>(red, green_sample.g, blue, green_sample.a);
}
