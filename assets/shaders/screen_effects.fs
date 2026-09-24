/* Ripple and chromatic poison share one post-process pass. */
#ifdef EIK_WEB
varying vec2 fragTexCoord;
#define SAMPLE texture2D
#else
in vec2 fragTexCoord;
out vec4 finalColor;
#define SAMPLE texture
#endif

uniform sampler2D texture0;
uniform vec2 rippleCenter;
uniform float rippleProgress;
uniform float time;
uniform float aspect;
uniform float chromaIntensity;
uniform float chromaShift;

vec2 ripple_offset(vec2 uv) {
    if (rippleProgress <= 0.0) return vec2(0.0);
    vec2 p = uv - rippleCenter;
    p.x *= aspect;
    float distance = length(p);
    float delta = distance - 0.833333 * rippleProgress;
    float displacement = 0.033333 * (1.0 - rippleProgress)
        * sin(delta * 60.0 - time * 6.0) * exp(-abs(delta) * 20.0);
    vec2 direction = distance > 0.0001 ? p / distance : vec2(0.0);
    direction.x /= aspect;
    return direction * displacement;
}

void main() {
    vec2 uv = clamp(fragTexCoord + ripple_offset(fragTexCoord), 0.0, 1.0);
    float shake = sin(time * 15.0) * chromaShift * 0.25 * chromaIntensity;
    vec2 shifted = uv + vec2(shake, 0.0);
    float r = SAMPLE(texture0, clamp(shifted + vec2(chromaIntensity * chromaShift, 0.0), 0.0, 1.0)).r;
    vec4 middle = SAMPLE(texture0, clamp(shifted, 0.0, 1.0));
    float b = SAMPLE(texture0, clamp(shifted - vec2(chromaIntensity * chromaShift, 0.0), 0.0, 1.0)).b;
    vec4 color = vec4(r, middle.g, b, middle.a);
#ifdef EIK_WEB
    gl_FragColor = color;
#else
    finalColor = color;
#endif
}
