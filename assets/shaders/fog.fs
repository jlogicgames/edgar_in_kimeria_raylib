/* The platform header is prepended by render.c. */
#ifdef EIK_WEB
varying vec2 fragTexCoord;
#else
in vec2 fragTexCoord;
out vec4 finalColor;
#endif

uniform vec2 size;
uniform float time;

float hash(vec2 p) {
    p = 50.0 * fract(p * 0.3183099 + vec2(0.71, 0.113));
    return -1.0 + 2.0 * fract(p.x * p.y * (p.x + p.y));
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0)), u.x), u.y);
}

float fractal_noise(vec2 uv) {
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    float f = 0.5 * noise(uv * 8.0);
    uv = m * uv * 8.0;
    f += 0.25 * noise(uv);
    uv = m * uv;
    f += 0.125 * noise(uv);
    return 0.5 + 0.5 * f;
}

void main() {
    vec2 uv = fragTexCoord;
    uv.x *= size.x / max(size.y, 1.0);
    float a = pow(max(fractal_noise(uv + vec2(time * 0.015, 0.0)), 0.0), 1.8) * 0.42;
    vec4 color = vec4(vec3(0.8, 0.4, 1.0) * a, a);
#ifdef EIK_WEB
    gl_FragColor = color;
#else
    finalColor = color;
#endif
}
