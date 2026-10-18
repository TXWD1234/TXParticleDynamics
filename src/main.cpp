// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#include "Application.hpp"
#include "impl/overlay.hpp"
#include "impl/ring_buffer.hpp"
#include "infrastructure.hpp"
#include "tx/type_traits.hpp"
#include <memory>

//template class tx::impl::OverlayMMW<tx::impl::RingBufferOverlayBase, int>;

struct Functor {
	void operator()(int) {}
};

template class tx::TaskThread<int, Functor>;

int main() {
	return application();
}
