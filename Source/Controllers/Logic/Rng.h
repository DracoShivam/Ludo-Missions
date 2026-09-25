#pragma once

#include <cstdint>
#include <random>

namespace lm {

class Rng {
public:
	explicit Rng(uint32_t seed = 0) { this->seed(seed); }
	void seed(uint32_t s) { m_engine.seed(s == 0 ? std::random_device{}() : s); }
	int dice() { return range(1, 6); }
	int range(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(m_engine); }
	double unit() { return std::uniform_real_distribution<double>(0.0, 1.0)(m_engine); }
	uint32_t next() { return m_engine(); }

private:
	std::mt19937 m_engine;
};

}  // namespace lm
