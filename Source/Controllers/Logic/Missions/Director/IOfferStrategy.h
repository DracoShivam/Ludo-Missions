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
	// mustOffer: the player currently has no live mission, so returning nullopt leaves them with nothing. A strategy
	// should drop its quality bar rather than decline.
	virtual std::optional<size_t> choose(const std::vector<CompiledMissionPtr>& candidates, const EvalContext& ctx, OfferMoment moment,
										 const OfferStats& stats, Rng& rng, bool mustOffer = false) = 0;
	virtual void onResolved(const std::string& id, bool completed) {}
	// Target solved for candidate `index` in the most recent choose(). 0 = use the authored target.
	virtual int solvedTarget(size_t index) const { return 0; }
	// Simulated completion probability for candidate `index` in the most recent choose().
	// Negative means "not measured" -- a strategy that does not simulate says so rather than lying.
	virtual double lastProbability(size_t index) const { return -1.0; }
};

}  // namespace lm
