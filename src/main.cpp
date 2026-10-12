#include "Project.hpp"
#include "impl/geometry.hpp"
#include "impl/utility/rgb.hpp"
#include "utility.hpp"
#include <GLFW/glfw3.h>
#include <random>
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
			case GLFW_KEY_SPACE:
				engine.createObject(tx::vec2{ 2.0f, 2.0f }, tx::vec2{ -0.2f, -0.2f });
				break;
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

		std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<tx::f32> posDist(-1.0f, 1.0f);
		std::uniform_real_distribution<tx::f32> velDist(-0.001f, 0.001f);

		// for (tx::u32 i = 0; i < 2000; i++) {
		// 	engine.createObject(tx::Origin, { velDist(rng), velDist(rng) });
		// }
		for (tx::u32 i = 0; i < 2000; i++) {
			engine.createObject({ posDist(rng), posDist(rng) });
		}

		// engine.createObject({ 1.0f, 1.0f });
		// engine.createObject({ -1.0f, -1.0f });

		return 1;
	}

	re::RE re;
	re::RSP rr;

	tx::ParticleDynamics::Engine engine;

	void update() {
		if (glfwGetMouseButton(framework.getWindow(), GLFW_MOUSE_BUTTON_LEFT)) {
			engine.applyImpulse(tx::glfwGetCursorPos(framework.getWindow()), 0.4f, 0.001f);
		}
		engine.update();
	}
	void render() {
		engine.draw([this](std::span<tx::vec2> pos) {
			rr.drawSprites(pos.size(), [&](re::Renderer::SpriteAtrribs& attribs) {
				std::copy(pos.begin(), pos.end(), attribs.positionBuffer.begin());
				std::fill(attribs.textureHandleBuffer.begin(), attribs.textureHandleBuffer.end(), 0);
				std::fill(attribs.textureIndexBuffer.begin(), attribs.textureIndexBuffer.end(), 0);
				std::fill(attribs.scaleBuffer.begin(), attribs.scaleBuffer.end(), tx::vec2{ 0.016f, 0.016f });
				std::fill(attribs.rotationBuffer.begin(), attribs.rotationBuffer.end(), re::mat2{});
				//std::fill(attribs.colorBuffer.begin(), attribs.colorBuffer.begin() + 5000, tx::White.compress());
				//std::fill(attribs.colorBuffer.begin() + 5000, attribs.colorBuffer.end(), tx::Red.compress());
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
