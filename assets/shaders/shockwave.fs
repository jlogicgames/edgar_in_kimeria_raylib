/* The platform header is prepended by render.c. */
#ifdef EIK_WEB
varying vec2 fragTexCoord;
#else
in vec2 fragTexCoord;
out vec4 finalColor;
#endif

uniform vec2 size;
uniform vec2 center;
uniform float time;
uniform float progress;

void main() {
    vec2 p = fragTexCoord - center;
    p.x *= size.x / max(size.y, 1.0);
    float ramp = smoothstep(0.02, 1.0, progress);
    float radius = 0.5 * ramp;
    float ripple = 0.0064 * sin(40.0 * length(p) - 6.0 * progress + time * 6.0);
    float width = max(0.008, 0.0625 * (1.0 - progress * 0.8));
    float ring = 1.0 - smoothstep(0.5 * width, 1.5 * width,
        abs(length(p) - radius + ripple));
    float alpha = clamp(ring * ramp * (1.0 - progress), 0.0, 1.0);
    vec4 color = vec4(1.0, 0.95, 0.7, alpha);
#ifdef EIK_WEB
    gl_FragColor = color;
#else
    finalColor = color;
#endif
}
