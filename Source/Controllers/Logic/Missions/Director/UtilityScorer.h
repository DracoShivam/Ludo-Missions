#pragma once

#include <string>

#include "Controllers/Logic/Missions/Director/IOfferStrategy.h"
#include "Models/GameConfig.h"

namespace lm {

// Stage 3 (docs/PLAN.md §7.7): U = weight/10 * fit * timely * novelty.
double missionUtility(const MissionDef& def, double p, int minTurns, double center, double halfWidth, const UtilityConfig& u,
					  const OfferStats& stats);

}  // namespace lm
