#pragma once

#include <string>

namespace lm {

struct MissionInstance {
	int uid = 0;  // unique per offer
	std::string id;
	std::string title;
	std::string description;  // rendered on every update
	int progress = 0;
	int target = 1;
	int turnsLeft = 0;
	int turns = 0;
	int rewardCoins = 0;
};

struct MissionUpdate {
	enum class Kind { Offered, Progress, Completed, Failed, Voided };
	Kind kind = Kind::Offered;
	MissionInstance instance;
};

inline const char* missionUpdateKindName(MissionUpdate::Kind k) {
	switch (k) {
		case MissionUpdate::Kind::Offered: return "Offered";
		case MissionUpdate::Kind::Progress: return "Progress";
		case MissionUpdate::Kind::Completed: return "Completed";
		case MissionUpdate::Kind::Failed: return "Failed";
		case MissionUpdate::Kind::Voided: return "Voided";
	}
	return "?";
}

}  // namespace lm
