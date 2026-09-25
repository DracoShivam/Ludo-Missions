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

}  // namespace lm
