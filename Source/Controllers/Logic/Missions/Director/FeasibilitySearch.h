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
// targetOverride > 0 retunes the objective before searching. For a ranged mission pass targetMin: if even the easiest
// variant cannot finish in the window, no variant can, and the mission is genuinely infeasible.
// maxMillis > 0 also bounds the search by wall clock. An expansion copies a MatchState and a MissionTracker, so
// a high expansion cap on an open board can run for a hundred milliseconds -- far past any per-decision budget.
// Running out of either budget yields Unknown, never Infeasible, so a mission is never dropped for being slow.
SearchResult feasibilitySearch(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
							   int maxExpansions, int targetOverride = 0, double maxMillis = 0);

}  // namespace lm
