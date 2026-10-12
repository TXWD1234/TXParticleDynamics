#pragma once
#include "tx/math.h"
#include <bits/stdc++.h>
#include <cmath>

namespace tx {

// ################################################
// $$$$$$$$$$$$$$$$ Infrasturcture $$$$$$$$$$$$$$$$
// ################################################

inline f32 hypot(vec2 v) { return std::hypot(v.x, v.y); }


namespace ParticleDynamics {
// ########################################
// $$$$$$$$$$$$$$$$ Engine $$$$$$$$$$$$$$$$
// ########################################
/**
 * WE LOVE VERLET
 */

struct EngineParameters {
	u32 maxEntityCount = 10000;
	struct Config {
		f32 ObjectRadius = 0.008f;
		f32 Gravity = 0.0001f;
		f32 Friction = 0.999f;
		f32 Restitution = 0.9f;
	} config;
};


class Engine {
public:
	Engine(EngineParameters param = EngineParameters{})
	    : m_data(
	          std::vector<vec2>(param.maxEntityCount),
	          std::vector<vec2>(param.maxEntityCount)),
	      m_config(param.config) {}

public:
	// ################ Main Loop Interface ################

	void update() {
		update_impl(8);
	}

	template <std::invocable<const std::span<vec2>> Func>
	void draw(Func&& f) {
		f(std::span(m_data.pos.begin(),
		            m_data.pos.begin() + m_state.objectCount));
	}

public:
	// ################ Public Interface ################

	u32 createObject(vec2 pos, vec2 initVel = tx::Origin) {
		m_data.pos[m_state.objectCount] = pos;
		m_data.prevPos[m_state.objectCount] = pos - initVel;
		m_state.objectCount++;
		return m_state.objectCount - 1;
	}

	void applyImpulse(vec2 pos, f32 radius, f32 intensity) {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			if (isColliding(m_data.pos[i], pos, radius)) {
				vec2 nuv = tx::unify(m_data.pos[i] - pos);
				m_data.pos[i] += nuv * intensity;
			}
		}
	}

private:
	// ################ Runtime Data ################

	struct Data_impl {
		/**
		 * Everything is going to be in an arena in the future, but for this
		 * prototype, std::vector is fine
		 */
		std::vector<vec2> pos;
		std::vector<vec2> prevPos;
	} m_data;

	struct State_impl {
		u32 objectCount = 0;
	} m_state;

	EngineParameters::Config m_config;

private:
	// ################ Implementation Utility ################

	vec2 findVel_impl(u32 id) {
		return m_data.pos[id] - m_data.prevPos[id];
	}



private:
	// ################ Logic Utility ################

	bool isColliding(vec2 a, vec2 b, f32 dist) {
		return sq(a.x - b.x) + sq(a.y - b.y) < sq(dist);
	}
	bool isColliding(u32 a, u32 b) {
		return isColliding(m_data.pos[a], m_data.pos[b],
		                   m_config.ObjectRadius * 2);
	}

private:
	// ################ Logic Implementation ################

	void update_impl(u32 substep) {
		f32 dt = 1.0f / static_cast<f32>(substep);
		while (substep--) {
			updateGravity_impl(dt);
			updateObject_impl();
			updateCollision_impl();
		}
	}

	// ================ Update Verlet ================

	void updateObject_impl() {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			vec2 vel = findVel_impl(i);
			m_data.prevPos[i] = m_data.pos[i];
			m_data.pos[i] += vel * m_config.Friction;
		}
	}

	// ================ Update Gravity ================

	void updateGravity_impl(f32 dt) {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			m_data.pos[i] += tx::unify(tx::Origin - m_data.pos[i]) * m_config.Gravity * dt;
		}
	}

	// ================ Update Collision ================

	void updateCollision_impl() {
		for (u32 i = 0; i < m_state.objectCount; i++) {
			for (u32 j = i + 1; j < m_state.objectCount; j++) {
				if (isColliding(i, j))
					solveCollision_impl(i, j);
			}
		}
	}
	// Collision Solver, the core physics logic
	void solveCollision_impl(u32 a, u32 b) {
		vec2 diff = m_data.pos[a] - m_data.pos[b];
		f32 dist = tx::hypot(diff);
		vec2 nuv = tx::unify(diff, dist);

		// push apart
		f32 overlap = (m_config.ObjectRadius * 2 - dist) * 0.5f;
		vec2 push = overlap * nuv;

		m_data.pos[a] += push;
		m_data.pos[b] -= push;

		// calculate impulse
		vec2 relVel = findVel_impl(a) - findVel_impl(b); // relative velocity
		f32 nRelVel = dot(relVel, nuv);

		if (nRelVel >= -0.0001f) return;

		// solve bounce
		float bounceImpulse = nRelVel * m_config.Restitution;
		vec2 impulseVec = nuv * bounceImpulse;
		m_data.prevPos[a] += impulseVec;
		m_data.prevPos[b] -= impulseVec;
	}
};



} // namespace ParticleDynamics
} // namespace tx