// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

// #include "Application.hpp"
#define SOL_ALL_SAFETIES_ON 1
#include "sol/sol.hpp"

//template class tx::impl::OverlayMMW<tx::impl::RingBufferOverlayBase, int>;

int main() {

	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
	lua["hello"] = []() {
		std::cout << "Hello World!\n";
	};
	lua.script("hello()");


	//return application();
}
