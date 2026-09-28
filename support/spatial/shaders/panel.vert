#version 440
layout(location = 0) in vec2 position;
layout(location = 0) out vec2 uv;
layout(std140, binding = 0) uniform Params {
    mat4 target;
    vec4 dimensions;
    vec4 shape;
    vec4 material;
    vec4 reflection;
    vec4 shadow;
} u;
void main() {
    uv = (position * (u.dimensions.zw + 2.0 * u.shape.z) - u.shape.z) / u.dimensions.zw;
    gl_Position = u.target * vec4(position, 0.0, 1.0);
}
