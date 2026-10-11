#include "Project.hpp"
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

		return 1;
	}

	re::RE re;
	re::RSP rr;

	void update() {
	}
	void render() {

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
