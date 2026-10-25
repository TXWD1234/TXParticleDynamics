// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "Project.hpp"
#include "impl/io_file.hpp"
#include "infrastructure.hpp"
#include "render_engine.hpp"
#include "lua_application.hpp"
#include "utility.hpp"
#include "impl/geometry.hpp"
#include "tx/json.h"
#include <iostream>
#include <optional>

class Application {
private:
	struct UpdateFunc {
		Application* ptr;
		inline void operator()() {
			ptr->update();
		}
	};
	struct RenderFunc {
		Application* ptr;
		inline void operator()() {
			ptr->render();
		}
	};
	tx::RE::Framework<
	    re::Mode::FixTickRate | re::Mode::PrintFrameRate,
	    UpdateFunc, RenderFunc>
	    framework;

	bool initGLFW() {
		std::cout << "Initializing GLFW...\n";
		if (!glfwInit()) {
			std::cerr << "[FatalError]: Failed to init GLFW\n";
			return false;
		}
		return true;
	}
	bool initGLAD() {
		std::cout << "Initializing GLAD...\n";
		if (!gl::init((void*)glfwGetProcAddress)) {
			std::cerr << "[FatalError]: Failed to init GLAD\n";
			return false;
		}
		return true;
	}

	bool m_valid = 0;

public:
	Application() {
		if (!initGLFW()) return;
		framework = decltype(framework){
			UpdateFunc{ this },
			RenderFunc{ this }
		};
		if (!framework.valid()) return;
		if (!initGLAD()) return;
		if (!init()) return;
		m_valid = 1;
	}
	~Application() {
		glfwTerminate();
	}

	void run() { this->framework.run(); }
	bool valid() const { return m_valid; }

private:
	void onKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (action == GLFW_PRESS || action == GLFW_REPEAT) {
			switch (key) {
			}
		}
	}

private:
	bool init() {
		tx::checkHardwareSupport();
		tx::glfwSetKeyCallback<Application, &Application::onKeyEvent>(framework.getWindow(), this);
		tx::glBasicSettings();
		stbi_set_flip_vertically_on_load(true);
		glfwSwapInterval(0); // turn off vsync

		std::string demoPath;
		{
			tx::JsonDocument doc(tx::readWholeFileText("config/config.json"));
			tx::u32 index = doc.root().find("script");
			if (tx::valid(index) && doc.root().at(index).is_str()) {
				demoPath = doc.root().at(index).get_str_unchecked();
			} else {
				std::cerr << "[Error] Failed to read config.json. Entry \"script\""
				             " does not exist or is not string type.\n";
				return 0;
			}
		}
		lua.emplace(demoPath);
		lua->configure([this](tx::ParticleDynamics::LuaApplication::EngineArg engineArg,
		                      tx::ParticleDynamics::LuaApplication::RenderEngineArg reArg)
		                   -> tx::ParticleDynamics::EngineCollection {
			engine.emplace(engineArg.param, engineArg.config);
			re.emplace(reArg.param, reArg.config);
			return { &engine.value(), &re.value() };
		});
		lua->init();

		return 1;
	}



	std::optional<tx::ParticleDynamics::Engine> engine;
	std::optional<tx::ParticleDynamics::RenderEngine> re;
	std::optional<tx::ParticleDynamics::LuaApplication> lua;

	void update() {
		lua->runtime();
		engine->update();
	}
	void render() {
		engine->draw([this](std::span<tx::vec2> _) { re->draw(_); });
	}
};

inline int application() {
	std::cout << "Initializing Application...\n";
	Application app;
	if (!app.valid()) {
		std::cerr << "[FatalError]: Failed to init Application\n";
		return 1;
	}

	std::cout << "[Status]: Main Loop Starts\n";
	app.run();

	std::cout << "[Status]: Terminating...\n";
	return 0;
}