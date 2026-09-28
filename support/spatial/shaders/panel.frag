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
float roundedDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 q = abs(point - u.dimensions.zw * 0.5) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}
void main() {
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec2 span = clamp(vec2(length(dx * u.dimensions.xy), length(dy * u.dimensions.xy)) - 1.0, 0.0, 1.0);
    dx *= 0.25 * span.x; dy *= 0.25 * span.y;
    vec4 pixel = 0.25 * (samplePanel(uv - dx - dy) + samplePanel(uv + dx - dy)
                       + samplePanel(uv - dx + dy) + samplePanel(uv + dx + dy));
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
