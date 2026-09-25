#pragma once

#include <vector>

#include "Controllers/Logic/Rng.h"
#include "Models/MatchState.h"
#include "Models/MoveOption.h"

namespace lm {

// Heuristic bot (docs/PLAN.md §5.5). Pure and deterministic given the Rng.
class BotBrain {
public:
	static double score(const MatchState& s, const MoveOption& o);
	static MoveOption choose(const MatchState& s, const std::vector<MoveOption>& options, Rng& rng);
};

}  // namespace lm
