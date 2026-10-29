// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "Project.hpp"
#include "render_engine.hpp"
#include <memory>
#include <string_view>

namespace tx::ParticleDynamics {

struct EngineCollection {
	Engine* engine;
	RenderEngine* re;
};

class LuaApplication {
public:
	LuaApplication(
	    std::string_view path);
	~LuaApplication();

	LuaApplication(const LuaApplication& other) = delete;
	LuaApplication& operator=(const LuaApplication& other) = delete;
	LuaApplication(LuaApplication&& other) = delete;
	LuaApplication& operator=(LuaApplication&& other) = delete;

public:
	// ================ Engine Facing Interface ================

	struct EngineArg {
		EngineParameters param;
		EngineConfig config;
	};
	struct RenderEngineArg {
		RenderEngineParameters param;
		RenderEngineConfig config;
	};

public:
	// ================ Lua Runtime ================
	void configure(const std::function<EngineCollection(
	                   EngineArg, RenderEngineArg)>& f);
	void init();
	void runtime();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	std::filesystem::path m_path;

	void init_impl();
	void exposeEngine_impl(Engine*);
	void exposeEngineConfig_impl();
};
} // namespace tx::ParticleDynamics