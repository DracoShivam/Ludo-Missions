#pragma once

#include <string>
#include <vector>

#include "Models/Params.h"

namespace lm {

enum class OfferMoment { TurnStart = 0, AfterRoll = 1 };

// One mission as authored in missions.json (pure data).
struct MissionDef {
	std::string id;
	bool enabled = true;
	std::string title;
	std::string description;  // template: {target} {turns} {turnsLeft} {progress}
	int rewardCoins = 0;
	int turns = 1;
	int weight = 10;
	int cooldownTurns = 3;
	int maxPerMatch = 0;  // 0 = unlimited (counts offers)
	std::vector<OfferMoment> moments{OfferMoment::TurnStart};
	Spec offerWhen;  // default {"type":"always"}
	Spec objective;
};

}  // namespace lm
