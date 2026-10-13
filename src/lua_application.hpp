// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "Project.hpp"
#include <memory>
#include <string_view>

class LuaApplication {
public:
	LuaApplication();
	LuaApplication(
	    std::string_view path,
	    tx::ParticleDynamics::Engine& engine);
	~LuaApplication();

	LuaApplication(LuaApplication&& other);
	LuaApplication& operator=(LuaApplication&& other);

public:
	// ================ Public Interface ================
	void configure();
	void init();
	void runtime();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	std::filesystem::path m_path;
	tx::ParticleDynamics::Engine* m_engine = nullptr;


	void init_impl();
	void exposeEngine_impl();
	void exposeEngineConfig_impl();
};