#include "Project.hpp"
#include "tx/json.h"
#include <string_view>

template class tx::impl::OverlayAlias<tx::RingBufferOverlayBase, int>;

int main() {
	return 0;
}
