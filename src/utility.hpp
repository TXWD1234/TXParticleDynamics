#include <filesystem>
#include "tx/graphics.h"
#include "tx/resource.h"

namespace fs = std::filesystem;

namespace tx {

namespace RenderEngine {
const fs::path shaderPath = tx::getExeDir() / "assets/shaders";

inline std::string readShaderSource(const std::string& filename) {
	return readWholeFileText(shaderPath / filename);
}
} // namespace RenderEngine




template <class T, void (T::*Callback)(GLFWwindow*, int, int, int, int)>
inline void glfwSetKeyCallback(GLFWwindow* window, T* _this) {
	glfwSetWindowUserPointer(window, _this);

	glfwSetKeyCallback(
	    window,
	    [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		    T* obj = static_cast<T*>(glfwGetWindowUserPointer(window));
		    if (obj) {
			    (obj->*Callback)(window, key, scancode, action, mods);
		    }
	    });
}
inline void glEnableTransparent() {
	gl::enable(gl::enums::BLEND);
	gl::blendFunc(gl::enums::SRC_ALPHA, gl::enums::ONE_MINUS_SRC_ALPHA);
}
inline void glDisableDepthTest() {
	gl::disable(gl::enums::DEPTH_TEST);
}
inline void glBasicSettings() {
	glEnableTransparent();
	glDisableDepthTest();
}
} // namespace tx