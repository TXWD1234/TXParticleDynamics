#version 460 core

layout(std140, binding = 0) uniform Environment {
    float radius;
};
layout(std140, binding = 1) uniform Cam {
    vec2 camPos;
	vec2 viewportScale;
	vec2 viewportScaleInv;
	float zoom;
};
layout(std430, binding = 0) readonly buffer posBuffer {
	vec2 positions[];
};

const vec2 SquareVertexPositions[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

out flat vec2 center;
out vec2 position;

void main() {
	center = ((positions[gl_InstanceID] * zoom) - camPos) * viewportScale;
	position = center + SquareVertexPositions[gl_VertexID] * radius * zoom * viewportScale;
    gl_Position = vec4(position, 0.0, 1.0);
}