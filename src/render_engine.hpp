// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/utility/rgb.hpp"
#include "tx/graphics.h"
#include "utility.hpp"

namespace tx::ParticleDynamics {

struct RenderEngineParameters {
	struct ColorConfig {
		bool Enabled = false;
		u32 IterationRange = 720;
	} Color{};
};
struct RenderEngineConfig {
	bool Isometric = false;

	f32 ObjectRadius = 0.008f;
};

class RenderEngine {
public:
	RenderEngine(RenderEngineParameters param, RenderEngineConfig config) {
		m_config = config;
		re.init();
		rr = re.createSectionProxy(
		    re::readShaderSource("circle.vert"),
		    re::readShaderSource("circle.frag"));
		initColorBuffer_impl(param.Color);
		m_derivedConfig.ColorEnabled = param.Color.Enabled;
	}

	RenderEngine(const RenderEngine& other) = delete;
	RenderEngine& operator=(const RenderEngine& other) = delete;
	RenderEngine(RenderEngine&& other) = delete;
	RenderEngine& operator=(RenderEngine&& other) = delete;

private:
	// ================ Config ================

	RenderEngineConfig m_config;
	struct {
		bool ColorEnabled = false;
	} m_derivedConfig;

private:
	// ================ Color ================
	std::vector<u32> colorBuffer;

	void initColorBuffer_impl(RenderEngineParameters::ColorConfig config) {
		if (!config.Enabled) return;
		colorBuffer.reserve(config.IterationRange);
		tx::generateRainbow(config.IterationRange, [this](tx::RGB rgb) {
			colorBuffer.push_back(rgb.compress());
		});
	}


public:
	void draw(std::span<vec2> pos) {
		rr.drawSprites(pos.size(), [this, pos](re::Renderer::SpriteAtrribs& attribs) {
			if (m_config.Isometric) {
				for (tx::u32 i = 0; i < pos.size(); i++) {
					attribs.positionBuffer[i] = { pos[i].x, pos[i].y * 0.5f };
				}
			} else {
				std::copy(pos.begin(), pos.end(), attribs.positionBuffer.begin());
			}
			std::fill(attribs.textureHandleBuffer.begin(), attribs.textureHandleBuffer.end(), 0);
			std::fill(attribs.textureIndexBuffer.begin(), attribs.textureIndexBuffer.end(), 0);
			std::fill(attribs.scaleBuffer.begin(), attribs.scaleBuffer.end(),
			          tx::vec2{ m_config.ObjectRadius * 2, m_config.ObjectRadius * 2 });
			std::fill(attribs.rotationBuffer.begin(), attribs.rotationBuffer.end(), re::mat2{});
			//std::fill(attribs.colorBuffer.begin(), attribs.colorBuffer.begin() + 5000, tx::White.compress());
			//std::fill(attribs.colorBuffer.begin() + 5000, attribs.colorBuffer.end(), tx::Red.compress());

			if (m_derivedConfig.ColorEnabled) {
				u32 size = attribs.colorBuffer.size();
				while (size >= colorBuffer.size()) {
					std::copy(colorBuffer.begin(), colorBuffer.end(),
					          attribs.colorBuffer.end() - size);
					size -= colorBuffer.size();
				}
				std::copy(colorBuffer.begin(), colorBuffer.begin() + size, attribs.colorBuffer.end() - size);
			} else
				std::fill(attribs.colorBuffer.begin(), attribs.colorBuffer.end(), tx::White.compress());
		});
		re.draw();
	}

private:
	re::RE re;
	re::RSP rr;
};
} // namespace tx::ParticleDynamics