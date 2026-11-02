// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/geometry.hpp"
#include "impl/gl_core/draw_call_manager.hpp"
#include "impl/gl_core/fence_manager.hpp"
#include "impl/gl_core/shader.hpp"
#include "impl/gl_core/shader_utils.hpp"
#include "impl/gl_core/uniform.hpp"
#include <exception>
#include <stdexcept>

namespace tx::ParticleDynamics {
namespace re {
using namespace tx::RenderEngine;

struct RendererParameters {
	u32 MaxEntityCount = 8192;
	f32 radius = 0.5f;
};

namespace impl::details {
constexpr const char* VertexSource = R"(#version 460 core

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
})";
constexpr const char* FragSource = R"(#version 460 core

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
})";
} // namespace impl::details

// fast circle renderer
// Using the Simple Pipeline
class Renderer {
public:
	Renderer(RendererParameters param)
	    : m_shader([] {
		      std::optional<ShaderProgram> program = re::buildShader(
		          impl::details::VertexSource, impl::details::FragSource);
		      if (!program) throw std::runtime_error("Failed to build shaders");
		      return std::move(*program);
	      }()),
	      dcm(m_shader),
	      m_posBuffer(SSBO<vec2>::DefaultBufferCount, param.MaxEntityCount) {
		dcm.drawMode = gl::enums::TRIANGLE_STRIP;
		dcm.begin = 0;
		dcm.count = 4;

		m_envUniform.binding = 0;
		m_camUniform.binding = 1;

		m_envUniform.radius = param.radius;
		m_envUniform.sync(FMSubmiter{ fm }, FMSubmiter{ fm });
		m_camUniform.sync(FMSubmiter{ fm }, FMSubmiter{ fm });
	}

	void draw(std::span<const vec2> data) {
		m_posBuffer.sync(data, 0, FMSubmiter{ fm }, FMSubmiter{ fm });
		dcm.instanceCount = data.size();
		draw_impl();
	}

	// ================ Camera System ================

	void updateCamera(vec2 pos, vec2 viewportScale, f32 zoom) {
		m_camUniform.pos = pos;
		m_camUniform.viewportScale = viewportScale;
		m_camUniform.viewportScaleInv = tx::vec2(1.0f / viewportScale.x, 1.0f / viewportScale.y);
		m_camUniform.zoom = zoom;
		m_camUniform.sync(FMSubmiter{ fm }, FMSubmiter{ fm });
	}

private:
	// ================ Member Variables ================

	ShaderProgram m_shader;

	SDCM dcm;
	FM fm;

	struct alignas(16) EnvUniform {
		f32 radius = 1.0f;
	};
	UniformObject<EnvUniform> m_envUniform;

	struct alignas(16) CameraUniform {
		vec2 pos = tx::Origin;
		vec2 viewportScale = vec2{ 1.0f, 1.0f };
		vec2 viewportScaleInv = vec2{ 1.0f, 1.0f };
		f32 zoom = 2.0f;
	};
	UniformObject<CameraUniform> m_camUniform;

	SSBO<vec2> m_posBuffer;

private:
	// ================ Draw Call Implementation ================

	void draw_impl() {
		dcm.draw();
		fm.update();
	}
};
} // namespace re
} // namespace tx::ParticleDynamics