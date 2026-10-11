#version 460 core
#extension GL_ARB_gpu_shader_int64 : require

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

layout(location = 0) in vec2  meshPosition;
layout(location = 1) in vec2  meshUV;
layout(location = 2) in vec2  instancePosition;
layout(location = 3) in uvec2 instanceTextureHandle;
layout(location = 4) in float instanceTextureIndex;
layout(location = 5) in vec2  instanceScale;
layout(location = 6) in mat2  instanceRotation;
layout(location = 8) in uint  instanceColor;

out vec2 uv;
out flat uint64_t textureHandle;
out flat float textureIndex;
out flat uint colorTint;

void main() {
	uv = meshUV;
	textureHandle = packUint2x32(instanceTextureHandle);
	textureIndex = instanceTextureIndex;
	colorTint = instanceColor;
    gl_Position = vec4(instanceRotation * (instanceScale * meshPosition) + instancePosition, 0.0, 1.0);
}