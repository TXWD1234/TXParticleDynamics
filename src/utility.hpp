// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
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
inline vec2 glfwGetCursorPos(GLFWwindow* window) {
	static int width;
	static int height;
	static bool inited = [&]() {
		glfwGetWindowSize(window, &width, &height);
		width *= 0.5;
		height *= 0.5;
		return 1;
	}();
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	x = (x - width) / (width);
	y = -(y - height) / (height);
	return vec2(x, y);
}
constexpr int GLFW_NUM_KEYS[] = {
	GLFW_KEY_0,
	GLFW_KEY_1,
	GLFW_KEY_2,
	GLFW_KEY_3,
	GLFW_KEY_4,
	GLFW_KEY_5,
	GLFW_KEY_6,
	GLFW_KEY_7,
	GLFW_KEY_8,
	GLFW_KEY_9
};
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