#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Controllers/Logic/Missions/MissionParser.h"
#include "Controllers/Logic/Rng.h"

namespace lm {

struct OfferStats {
	std::map<std::string, int> offersThisMatch;
	std::string lastOfferedId;
	int humanRaceRank = 1;  // 1 = leading .. 4 = last
};

// Chooses which (if any) candidate mission to serve at an offer moment. Swappable (see docs/PLAN.md §7.7).
class IOfferStrategy {
public:
	virtual ~IOfferStrategy() = default;
	virtual std::optional<size_t> choose(const std::vector<CompiledMissionPtr>& candidates, const EvalContext& ctx, OfferMoment moment,
										 const OfferStats& stats, Rng& rng) = 0;
	virtual void onResolved(const std::string& id, bool completed) {}
};

}  // namespace lm
