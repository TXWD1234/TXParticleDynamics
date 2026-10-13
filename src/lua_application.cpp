// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#include "lua_application.hpp"
#include "Project.hpp"
#include "impl/io_file.hpp"
#include <exception>
#define SOL_ALL_SAFETIES_ON 1
#include "sol/sol.hpp"
#include <memory>

struct LuaApplication::Impl {
	sol::state lua;
	sol::table pd;
	sol::protected_function m_configureScript;
	sol::protected_function m_initScript;
	sol::protected_function m_runtimeScript;
};
LuaApplication::LuaApplication(
    std::string_view path,
    tx::ParticleDynamics::Engine& engine)
    : m_impl(std::make_unique<Impl>()),
      m_path(path), m_engine(&engine) { init_impl(); }
LuaApplication::LuaApplication() = default;
LuaApplication::~LuaApplication() = default;

LuaApplication::LuaApplication(LuaApplication&& other) = default;
LuaApplication& LuaApplication::operator=(LuaApplication&& other) = default;

void LuaApplication::configure() {
	exposeEngineConfig_impl();
	tx::ParticleDynamics::EngineParameters param;
	tx::ParticleDynamics::EngineConfig config;
	m_impl->pd["param"] = std::ref(param);
	m_impl->pd["config"] = std::ref(config);
	m_impl->m_configureScript();

	// init engine
	(*m_engine) = tx::ParticleDynamics::Engine(param, config);

	m_impl->pd["param"] = sol::lua_nil;
	m_impl->pd["config"] = std::ref(m_engine->m_config);

	exposeEngine_impl();
}
void LuaApplication::init() { m_impl->m_initScript(); }
void LuaApplication::runtime() { m_impl->m_runtimeScript(); }

void LuaApplication::init_impl() {
	auto loadScript = [this](std::string_view filename,
	                         sol::protected_function& func) {
		sol::load_result result = m_impl->lua.load(
		    tx::readWholeFileText(m_path / filename));

		if (!result.valid()) {
			sol::error err = result;
			std::cerr
			    << "[Error]: Failed to load script file "
			    << filename << ".\n"
			    << "Message: " << err.what() << '\n';
			std::terminate();
		} else
			func = result;
	};


	loadScript("configure.lua", m_impl->m_configureScript);
	loadScript("init.lua", m_impl->m_initScript);
	loadScript("runtime.lua", m_impl->m_runtimeScript);

	m_impl->lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
	m_impl->pd = m_impl->lua.create_named_table("pd");
}
// expose all APIs of the engine
void LuaApplication::exposeEngine_impl() {
	auto* engine = m_engine;
	m_impl->pd["createObject"] =
	    [engine](
	        tx::f32 x, tx::f32 y,
	        sol::optional<tx::f32> initVelX, sol::optional<tx::f32> initVelY) {
		    engine->createObject(
		        { x, y }, { initVelX.value_or(0.0f), initVelY.value_or(0.0f) });
	    };
	m_impl->pd["applyImpulse"] =
	    [engine](
	        tx::f32 x, tx::f32 y, tx::f32 radius, tx::f32 intensity) {
		    engine->applyImpulse(
		        { x, y }, radius, intensity);
	    };
}
void LuaApplication::exposeEngineConfig_impl() {
	m_impl->lua.new_usertype<tx::ParticleDynamics::EngineParameters>(
	    "EngineParameter",
	    "MaxEntityCount", &tx::ParticleDynamics::EngineParameters::MaxEntityCount,
	    "SubstepCount", &tx::ParticleDynamics::EngineParameters::SubstepCount);

	m_impl->lua.new_usertype<tx::ParticleDynamics::EngineConfig>(
	    "EngineConfig",
	    "ObjectRadius", &tx::ParticleDynamics::EngineConfig::ObjectRadius,
	    "Gravity", &tx::ParticleDynamics::EngineConfig::Gravity,
	    "Friction", &tx::ParticleDynamics::EngineConfig::Friction,
	    "Restitution", &tx::ParticleDynamics::EngineConfig::Restitution);
}