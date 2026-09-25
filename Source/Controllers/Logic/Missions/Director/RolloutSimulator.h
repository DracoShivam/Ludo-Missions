#pragma once

#include "Controllers/Logic/Missions/MissionParser.h"
#include "Controllers/Logic/Rng.h"
#include "Models/GameConfig.h"
#include "Models/MatchState.h"

namespace lm {

// Stage 2 (docs/PLAN.md §7.7): one Monte Carlo playout of the mission window with real random dice and real bots.
// The human plays a mission-seeking greedy policy. Returns true if the mission completed; turnsUsed = self turn-ends.
bool simulateMissionOnce(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
						 Rng& rng, int* turnsUsed = nullptr);

// Same playout, but reports how far the objective actually got instead of a yes/no. One set of these prices EVERY
// target at once -- P(complete at target t) is just the share of runs whose achieved value reached t -- which is what
// makes solving a target affordable. `targetForPolicy` is the ceiling the simulated player plays toward.
int simulateMissionAchieved(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
							Rng& rng, int targetForPolicy);

}  // namespace lm
