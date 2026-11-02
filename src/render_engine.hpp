// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "renderer.hpp"
#include "infrastructure.hpp"
#include "utility.hpp"
#include "impl/utility/rgb.hpp"
#include <GLFW/glfw3.h>

namespace tx::ParticleDynamics {

struct RenderEngineParameters {
	u32 MaxEntityCount = 10000;
	f32 ObjectRadius = 0.008f;
};
struct RenderEngineConfig {
	bool Isometric = false;
};

class RenderEngine {
public:
	RenderEngine(RenderEngineParameters param, RenderEngineConfig config, GLFWwindow* window)
	    : rr(re::RendererParameters{ param.MaxEntityCount, param.ObjectRadius }),
	      cc(window) { m_config = config; }

	RenderEngine(const RenderEngine& other) = delete;
	RenderEngine& operator=(const RenderEngine& other) = delete;
	RenderEngine(RenderEngine&& other) = delete;
	RenderEngine& operator=(RenderEngine&& other) = delete;

private:
	// ================ Config ================

	RenderEngineConfig m_config;

public:
	void draw(std::span<const vec2> pos) { rr.draw(pos); }
	void update(GLFWwindow* window) {
		cc.update(window, [this](tx::vec2 pos, tx::vec2 viewportScale, tx::f32 zoom) {
			rr.updateCamera(pos, viewportScale, zoom);
		});
	}

private:
	re::Renderer rr;
	tx::CameraController cc;
};
} // namespace tx::ParticleDynamics