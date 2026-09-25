#pragma once

#include <algorithm>

#include "Models/GameConfig.h"

namespace lm {

// Adaptive target success probability ("center"): completions make offers harder, failures easier.
class DifficultyTracker {
public:
	explicit DifficultyTracker(const DifficultyConfig& c = {}) : m_cfg(c), m_center(c.startCenter) {}
	double center() const { return m_center; }
	void setCenter(double c) { m_center = std::clamp(c, m_cfg.min, m_cfg.max); }
	void onResolved(bool completed) { setCenter(m_center + (completed ? m_cfg.stepOnComplete : m_cfg.stepOnFail)); }
	void reset() { m_center = m_cfg.startCenter; }
	// Effective center for this offer (falling behind in the race -> easier).
	double effectiveCenter(int humanRaceRank) const {
		return humanRaceRank >= 4 ? std::clamp(m_center + m_cfg.behindBias, m_cfg.min, m_cfg.max) : m_center;
	}

private:
	DifficultyConfig m_cfg;
	double m_center;
};

}  // namespace lm
