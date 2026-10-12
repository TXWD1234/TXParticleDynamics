#include "Project.hpp"
#include "impl/geometry.hpp"
#include "impl/utility/rgb.hpp"
#include "utility.hpp"
#include "tx/json.h"
#include "tx/type_traits.hpp"
#include <string_view>
#include <iostream>

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
	tx::RE::Framework<re::Mode::FixTickRate | re::Mode::PrintFrameRate, UpdateFunc, RenderFunc> framework;

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
		tx::glfwSetKeyCallback<Application, &Application::onKeyEvent>(framework.getWindow(), this);
		tx::glBasicSettings();
		stbi_set_flip_vertically_on_load(true);
		glfwSwapInterval(0); // turn off vsync

		re.init();
		rr = re.createSectionProxy(
		    re::readShaderSource("circle.vert"),
		    re::readShaderSource("circle.frag"));

		tx::u32 count = 0;
		for (tx::f32 i = -0.8; i < 0.8f; i += 0.042f) {
			for (tx::f32 j = -0.8; j < 0.8f; j += 0.042f) {
				engine.createObject(tx::vec2{ i, j });
				count++;
			}
		}

		std::cout << count << '\n';
		// engine.createObject({ 1.0f, 1.0f });
		// engine.createObject({ -1.0f, -1.0f });

		return 1;
	}

	re::RE re;
	re::RSP rr;

	tx::ParticleDynamics::Engine engine;

	void update() {
		engine.update();
	}
	void render() {
		engine.draw([this](std::span<tx::vec2> pos) {
			rr.drawSprites(pos.size(), [&](re::Renderer::SpriteAtrribs& attribs) {
				std::copy(pos.begin(), pos.end(), attribs.positionBuffer.begin());
				std::fill(attribs.textureHandleBuffer.begin(), attribs.textureHandleBuffer.end(), 0);
				std::fill(attribs.textureIndexBuffer.begin(), attribs.textureIndexBuffer.end(), 0);
				std::fill(attribs.scaleBuffer.begin(), attribs.scaleBuffer.end(), tx::vec2{ 0.02f, 0.02f });
				std::fill(attribs.rotationBuffer.begin(), attribs.rotationBuffer.end(), re::mat2{});
				std::fill(attribs.colorBuffer.begin(), attribs.colorBuffer.end(), tx::White.compress());
			});
		});
		re.draw();
	}

	void drawCircle_impl(tx::vec2 pos, float radius) {
		rr.drawSprite(pos, 0, 0.0f, { radius * 0.5f, radius * 0.5f });
	}
};

//template class tx::impl::OverlayMMW<tx::impl::RingBufferOverlayBase, int>;



int main() {
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
