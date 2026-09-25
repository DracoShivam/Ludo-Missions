#include "Controllers/Logic/Missions/Director/UtilityScorer.h"

#include <cmath>

namespace lm {

double missionUtility(const MissionDef& def, double p, int minTurns, double center, double halfWidth, const UtilityConfig& u,
					  const OfferStats& stats) {
	double z = (p - center) / std::max(1e-6, halfWidth);
	double fit = std::max(u.fitFloor, std::exp(-z * z));
	double timely = 1.0 + u.timelyBonus * (minTurns == 0 ? 1.0 : 0.0);
	auto it = stats.offersThisMatch.find(def.id);
	int offered = it == stats.offersThisMatch.end() ? 0 : it->second;
	double novelty = 1.0 / std::pow(1.0 + offered, u.noveltyPower);
	if (def.id == stats.lastOfferedId) novelty *= u.repeatPenalty;
	return (def.weight / 10.0) * fit * timely * novelty;
}

}  // namespace lm
