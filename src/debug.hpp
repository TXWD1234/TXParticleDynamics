// Copyright (c) 2026 TXParticleDynamics. Licensed under the MIT License.

#pragma once
#include "impl/geometry.hpp"
#include "tx/basic_types.hpp"
#include <mutex>
#include <utility>
#include <vector>
#include <fstream>
#include <string_view>

namespace tx::ParticleDynamics {

inline struct Debugger {
	struct Argument {
		vec2 v;
		u32 actual;
		u32 expected;
		u32 gridElementCount;
	};

	std::vector<Argument> data;
	std::mutex mtx;

	void push(Argument arg) {
		std::lock_guard<std::mutex> lock(mtx);
		data.push_back(arg);
	}

	void clear() {
		data.clear();
	}

	void save(std::string_view filepath) {
		std::ofstream ofs(filepath.data());

		std::sort(data.begin(), data.end(), [](Argument a, Argument b) {
			return a.expected < b.expected;
		});

		for (auto i : data) {
			ofs << i.v << ' ' << i.expected << ' ' << i.actual << '\n';
		}

		ofs.close();
	}

	void terminate() { std::abort(); }

} debugger;
} // namespace tx::ParticleDynamics