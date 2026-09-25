#pragma once

#include "Controllers/Logic/Missions/MissionParser.h"
#include "Models/GameConfig.h"
#include "Models/MatchState.h"

namespace lm {

struct SearchResult {
	enum class Verdict { Feasible, Infeasible, Unknown };
	Verdict verdict = Verdict::Unknown;
	int minTurns = 0;  // lower bound on human turn-ends needed (0 = can finish this very turn)
	int expansions = 0;
};

// Stage 1 (docs/PLAN.md §7.7): A* over (MatchState, MissionTracker) with CHOSEN dice (best case) and FROZEN bots.
// Edge cost = number of TURN_ENDED(self) events (0/1). Budget exhausted -> Unknown (never drop a mission wrongly).
SearchResult feasibilitySearch(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
							   int maxExpansions);

}  // namespace lm
