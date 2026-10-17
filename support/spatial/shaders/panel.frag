#version 440
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 1) uniform sampler2D source;
layout(std140, binding = 0) uniform Params {
    mat4 target;
    vec4 dimensions;
    vec4 shape;
    vec4 material;
    vec4 reflection;
    vec4 shadow;
} u;
vec4 samplePanel(vec2 coordinate) {
    vec2 texel = coordinate * u.dimensions.xy - 0.5;
    vec2 phase = fract(texel);
    phase = phase * phase * (3.0 - 2.0 * phase);
    return texture(source, (floor(texel) + phase + 0.5) / u.dimensions.xy);
}

vec4 cubicWeights(float t) {
    float t2 = t * t, t3 = t2 * t;
    return vec4(-0.5*t + t2 - 0.5*t3, 1.0 - 2.5*t2 + 1.5*t3,
                0.5*t + 2.0*t2 - 1.5*t3, -0.5*t2 + 0.5*t3);
}
vec4 sampleBoundedCubic(vec2 coordinate) {
    vec2 texel = coordinate * u.dimensions.xy - 0.5;
    vec2 cell = floor(texel), phase = fract(texel);
    vec4 wx = cubicWeights(phase.x), wy = cubicWeights(phase.y);
    vec4 value = vec4(0.0), low = vec4(1.0), high = vec4(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec4 pixel = texture(source, (cell + vec2(float(x)-0.5, float(y)-0.5)) / u.dimensions.xy);
            value += pixel * wx[x] * wy[y];
            if (x > 0 && x < 3 && y > 0 && y < 3) {
                low = min(low, pixel); high = max(high, pixel);
            }
        }
    }
    value = clamp(value, low, high);
    value.rgb = min(value.rgb, value.aaa);
    return value;
}
vec4 sampleFootprint(vec2 coordinate, vec2 dx, vec2 dy) {
    return 0.25 * (samplePanel(coordinate - dx - dy) + samplePanel(coordinate + dx - dy)
                 + samplePanel(coordinate - dx + dy) + samplePanel(coordinate + dx + dy));
}
float roundedDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 q = abs(point - u.dimensions.zw * 0.5) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}
void main() {
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec2 footprint = vec2(length(dx * u.dimensions.xy), length(dy * u.dimensions.xy));
    vec2 span = clamp(footprint - 1.0, 0.0, 1.0);
    dx *= 0.25 * span.x; dy *= 0.25 * span.y;

    float filtering = smoothstep(1.75, 2.5, max(footprint.x, footprint.y));
    vec4 pixel;
    if (filtering < 1.0)
        pixel = sampleBoundedCubic(uv);
    if (filtering > 0.0) {
        vec4 filtered = sampleFootprint(uv, dx, dy);
        pixel = filtering >= 1.0 ? filtered : mix(pixel, filtered, filtering);
    }
    if (u.shape.w > 0.0) {
        vec2 point = uv * u.dimensions.zw;
        vec2 halfSize = max(vec2(0.0), (u.dimensions.zw - 1.0) * 0.5);
        float radius = min(u.shape.x, min(halfSize.x, halfSize.y));
        float distance = roundedDistance(point, halfSize, radius);
        float aa = max(0.0001, 0.5 * fwidth(distance));
        float coverage = 1.0 - smoothstep(-aa, aa, distance);
        vec2 diagonal = max(vec2(1.0), u.dimensions.zw - 1.0);
        float t = clamp(dot(point - 0.5, diagonal) / dot(diagonal, diagonal), 0.0, 1.0);
        float alpha = max(0.0, u.material.a - 0.12 * t) * u.shape.y;
        pixel = (pixel + vec4(u.material.rgb * alpha, alpha) * (1.0 - pixel.a)) * coverage;
        float rim = 1.0 - smoothstep(0.5 - aa, 0.5 + aa, abs(distance));
        float rimAlpha = (t < 0.35 ? mix(u.reflection.a, 0.12, t / 0.35)
                          : mix(0.12, 0.0, (t - 0.35) / 0.65)) * u.shape.y * rim;
        pixel = vec4(u.reflection.rgb * rimAlpha, rimAlpha) + pixel * (1.0 - rimAlpha);
        float shadowDistance = max(0.0, roundedDistance(point - vec2(0.0, 3.0), halfSize, radius));
        float shadowAlpha = exp(-shadowDistance * shadowDistance / 50.0) * u.shadow.a
                            * u.shape.y * (1.0 - coverage);
        pixel += vec4(u.shadow.rgb * shadowAlpha, shadowAlpha) * (1.0 - pixel.a);
    }
    color = pixel;
}
