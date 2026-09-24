/* A compact GLSL port of the bomb's procedural blast. */
#ifdef EIK_WEB
varying vec2 fragTexCoord;
#else
in vec2 fragTexCoord;
out vec4 finalColor;
#endif

uniform float time;
uniform float progress;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0)), f.x), f.y);
}

void main() {
    vec2 p = fragTexCoord - vec2(0.5, 0.42);
    float t = max(progress, 0.01);
    float shape = 1.0 - dot(p, p) / (t * 0.55) - t * 1.5;
    float turbulent = noise(p * 8.0 + time * 0.6) * 0.34;
    float a = smoothstep(0.0, 0.12, shape + turbulent);
    a *= 1.0 - smoothstep(0.0, 0.18, p.y + 0.5);
    vec3 color = mix(vec3(0.2, 0.15, 0.3), vec3(0.95, 0.55, 0.1), t);
    vec4 result = vec4(color, a * (1.0 - progress));
#ifdef EIK_WEB
    gl_FragColor = result;
#else
    finalColor = result;
#endif
}
