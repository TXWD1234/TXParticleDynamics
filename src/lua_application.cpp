// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#include "lua_application.hpp"
#include "impl/io_file.hpp"
#define SOL_ALL_SAFETIES_ON 1
#include "sol/sol.hpp"
#include <memory>

struct LuaApplication::Impl {
	sol::state lua;
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

void LuaApplication::init() { m_impl->lua.script(tx::readWholeFileText(m_path / "init.lua")); }
void LuaApplication::runtime() { m_impl->m_runtimeScript(); }

void LuaApplication::init_impl() {
	sol::load_result result = m_impl->lua.load(
	    tx::readWholeFileText(m_path / "runtime.lua"));

	if (!result.valid())
		sol::error err = result;
	else
		m_impl->m_runtimeScript = result;

	m_impl->lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);

	exposeEngine_impl();
}
void LuaApplication::exposeEngine_impl() {
	sol::table pd = m_impl->lua.create_named_table("pd");

	pd["createObject"] =
	    [this](
	        tx::f32 x, tx::f32 y,
	        sol::optional<tx::f32> initVelX, sol::optional<tx::f32> initVelY) {
		    m_engine->createObject(
		        { x, y }, { initVelX.value_or(0.0f), initVelY.value_or(0.0f) });
	    };
}