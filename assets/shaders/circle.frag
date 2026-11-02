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
layout(location = 0) out vec4 FragColor;

in flat vec2 center;
in vec2 position;

void main() {

	float dist = length((center - position) * viewportScaleInv);
	float delta = fwidth(dist);
	float alpha = 1.0 - smoothstep(radius * zoom - delta, radius * zoom + delta, dist);

	FragColor = vec4(0.0, 1.0, 1.0, alpha);
}