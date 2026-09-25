#pragma once

#include "Models/MissionInstance.h"

namespace lm {

// MissionController -> views
struct MissionUpdated {
	static constexpr const char* NAME = "lm.mission.updated";
	MissionUpdate update;
};

// Published inside the MATCH_ENDED dispatch, before the Voided updates.
struct MissionMatchSummary {
	static constexpr const char* NAME = "lm.mission.matchSummary";
	int completed = 0;
	int failed = 0;
	int coinsEarned = 0;
};

}  // namespace lm
