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
	// Optional item reward, e.g. "kick". Missions are one source of powers; the powers feature knows nothing
	// about missions, so this is just an id handed onward, exactly like a coin amount.
	std::string rewardPower;
	int turns = 1;
	int weight = 10;
	int cooldownTurns = 3;
	int maxPerMatch = 0;  // 0 = unlimited (counts offers)
	std::vector<OfferMoment> moments{OfferMoment::TurnStart};
	Spec offerWhen;  // default {"type":"always"}
	Spec objective;
	// Authored difficulty range. 0 = fixed target, authored in the objective. When set, the Director solves the
	// actual target per offer so the same mission reads as "cut 1" on a quiet board and "cut 4" on a busy one.
	int targetMin = 0;
	int targetMax = 0;
};

}  // namespace lm
