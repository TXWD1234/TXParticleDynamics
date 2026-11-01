#version 460 core

layout(std430, binding = 0) readonly buffer posBuffer {
	vec2 positions[];
};

const vec2 SquareVertexPositions[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void main() {
    gl_Position = vec4(
		positions[gl_InstanceID] + SquareVertexPositions[gl_VertexID] * 0.5,
		0.0, 1.0);
}