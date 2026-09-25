#pragma once

#include "Controllers/Logic/Missions/Director/IOfferStrategy.h"

namespace lm {

// Fallback strategy: pick by designer weight.
class WeightedRandomStrategy : public IOfferStrategy {
public:
	std::optional<size_t> choose(const std::vector<CompiledMissionPtr>& candidates, const EvalContext&, OfferMoment, const OfferStats&,
								 Rng& rng, bool /*mustOffer*/ = false) override {
		if (candidates.empty()) return std::nullopt;
		int total = 0;
		for (auto& c : candidates) total += c->def.weight;
		int r = rng.range(1, total);
		for (size_t i = 0; i < candidates.size(); i++) {
			r -= candidates[i]->def.weight;
			if (r <= 0) return i;
		}
		return candidates.size() - 1;
	}
};

}  // namespace lm
