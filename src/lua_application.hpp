// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "Project.hpp"
#include "impl/io_file.hpp"
#include "tx/resource.h"
#define SOL_ALL_SAFETIES_ON 1
#include "sol/sol.hpp"
#include <string_view>



class LuaApplication {
public:
	LuaApplication() = default;
	LuaApplication(
	    std::string_view path,
	    tx::ParticleDynamics::Engine& engine)
	    : m_path(path), m_engine(&engine) {
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
		exposeEngine_impl();
	}

public:
	// ================ Public Interface ================
	void init() { lua.script(tx::readWholeFileText(m_path / "init.lua")); }
	void runtime() { lua.script(tx::readWholeFileText(m_path / "runtime.lua")); }

private:
	sol::state lua;
	std::filesystem::path m_path;
	tx::ParticleDynamics::Engine* m_engine;

	void exposeEngine_impl() {
		sol::table pd = lua.create_named_table("pd");

		pd["createObject"] =
		    [this](
		        tx::f32 x, tx::f32 y,
		        sol::optional<tx::f32> initVelX, sol::optional<tx::f32> initVelY) {
			    m_engine->createObject(
			        { x, y }, { initVelX.value_or(0.0f), initVelY.value_or(0.0f) });
		    };
	}
};