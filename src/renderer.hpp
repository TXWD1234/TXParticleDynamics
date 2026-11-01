// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/gl_core/draw_call_manager.hpp"
#include "impl/gl_core/fence_manager.hpp"
#include "impl/gl_core/gl_core.hpp"
#include "impl/gl_core/shader.hpp"
#include "impl/gl_core/shader_utils.hpp"
#include "impl/gl_core/uniform.hpp"
#include "tx/upset.hpp"

namespace tx::ParitcleDynamics {
namespace re {
using namespace tx::RenderEngine;
// fast circle renderer
// Using the Simple Pipeline
class Renderer {
public:
	Renderer(ShaderProgram&& shaderProgram)
	    : m_shader(std::move(shaderProgram)),
	      dcm(m_shader) {
		dcm.drawMode = gl::enums::TRIANGLE_STRIP;
		dcm.begin = 0;
		dcm.count = 4;
	}

	void draw(std::span<const vec2> data) {
		posBuffer.sync(data, 0, FMSubmiter{ fm }, FMSubmiter{ fm });
		dcm.instanceCount = data.size();
		draw_impl();
	}

private:
	// ================ Member Variables ================

	ShaderProgram m_shader;

	SDCM dcm;
	FM fm;

	//struct alignas(16) Uniform_impl {
	//
	//};
	//UniformBufferObject<Uniform_impl> uniform;
	SSBO<vec2> posBuffer;

private:
	// ================ Draw Call Implementation ================

	void draw_impl() {
		fm.update();
		dcm.draw();
	}
};
} // namespace re
} // namespace tx::ParitcleDynamics