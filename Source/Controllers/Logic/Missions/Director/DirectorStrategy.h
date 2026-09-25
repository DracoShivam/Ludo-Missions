#pragma once

#include <string>
#include <vector>

#include "Controllers/Logic/Missions/Director/DifficultyTracker.h"
#include "Controllers/Logic/Missions/Director/IOfferStrategy.h"
#include "Models/GameConfig.h"

namespace lm {

struct CandidateEval {
	std::string id;
	bool infeasible = false;
	bool unknown = false;
	int minTurns = 0;
	int runs = 0;
	int successes = 0;
	double p = 0;
	double utility = 0;
};

// The Mission Director (docs/PLAN.md §7.7): A* feasibility -> Monte Carlo P(success) -> utility -> softmax, with gate.
class DirectorStrategy : public IOfferStrategy {
public:
	DirectorStrategy(const DirectorConfig& cfg, const RulesConfig& rules) : m_cfg(cfg), m_rules(rules), m_difficulty(cfg.difficulty) {}
	std::optional<size_t> choose(const std::vector<CompiledMissionPtr>& candidates, const EvalContext& ctx, OfferMoment moment, const OfferStats& stats,
								 Rng& rng) override;
	void onResolved(const std::string& id, bool completed) override { m_difficulty.onResolved(completed); }

	DifficultyTracker& difficulty() { return m_difficulty; }
	const std::vector<CandidateEval>& lastEvals() const { return m_lastEvals; }
	const std::string& lastDecisionLog() const { return m_lastLog; }
	double lastElapsedMs() const { return m_lastMs; }

private:
	DirectorConfig m_cfg;
	RulesConfig m_rules;
	DifficultyTracker m_difficulty;
	std::vector<CandidateEval> m_lastEvals;
	std::string m_lastLog;
	double m_lastMs = 0;
};

}  // namespace lm
