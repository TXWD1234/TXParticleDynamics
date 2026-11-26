// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "tx/impl/geometry.hpp"
#include "tx/basic_types.hpp"
#include "tx/graphics.h"
#include "tx/impl/gl_core/fence_manager.hpp"
#include <iomanip>
#include <mutex>
#include <vector>
#include <fstream>
#include <string_view>


namespace tx::ParticleDynamics {

inline struct Debugger {
	struct Argument {
		f32 v;
	};

	std::vector<Argument> data;
	std::mutex mtx;

	void push(Argument arg) {
		std::lock_guard<std::mutex> lock(mtx);
		data.push_back(arg);
	}

	void clear() {
		data.clear();
	}

	void save(std::string_view filepath) {
		std::ofstream ofs(filepath.data());

		u32 counter = 0;
		for (auto i : data) {
			ofs << counter++ << '\t' << std::fixed << std::setprecision(7) << i.v << '\n';
		}

		ofs.close();
	}

	void terminate() { std::abort(); }

} debugger;

// Render debug lines
class DebugRenderer {
	// This renderer is just going to assume that camera uniform is bound at
	// binding 1.
public:
	DebugRenderer()
	    : m_shader([] {
		      std::optional<re::ShaderProgram> program = re::buildShader(
		          VertSource, FragSource);
		      if (!program) throw std::runtime_error("Failed to build shaders");
		      return std::move(*program);
	      }()),
	      dcm(m_shader) {
		dcm.drawMode = gl::enums::LINES;
		dcm.begin = 0;
		dcm.count = 2;
	}

	void drawLine(vec2 pa, vec2 pb, RGBA color = tx::White) {
		bufStage.push_back(Line{ pa, pb, color });
	}

	void draw() {
		if (bufStage.empty()) return;

		buf.sync(std::span<Line>(bufStage), 0,
		         re::FMSubmiter{ fm }, re::FMSubmiter{ fm });
		dcm.instanceCount = bufStage.size();
		bufStage.clear();

		dcm.draw();
		fm.update();
	}

private:
	re::ShaderProgram m_shader;

	re::SDCM dcm;
	re::FM fm;

	struct Line {
		vec2 a, b;
		RGBA color;
	};
	re::SSBO<Line> buf;
	std::vector<Line> bufStage;

private:
	static constexpr const char* VertSource = R"(#version 460 core

layout(std140, binding = 1) uniform Cam {
    vec2 camPos;
	vec2 viewportScale;
	vec2 viewportScaleInv;
	float zoom;
};

struct Line {
	vec2 pos[2];
	vec4 color;
};
layout(std430, binding = 0) readonly buffer posBuffer {
	Line lines[];
};

flat out int instanceId;

void main() {
	instanceId = gl_InstanceID;
	gl_Position = vec4(
		(lines[gl_InstanceID].pos[gl_VertexID] * zoom - camPos) * viewportScale,
		0.0, 1.0);
})";
	static constexpr const char* FragSource = R"(#version 460 core
struct Line {
	vec2 pos[2];
	vec4 color;
};
layout(std430, binding = 0) readonly buffer posBuffer {
	Line lines[];
};

out vec4 FragColor;

flat in int instanceId;

void main() {
	FragColor = lines[instanceId].color;
})";
};
} // namespace tx::ParticleDynamics