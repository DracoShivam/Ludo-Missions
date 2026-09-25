#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Controllers/Logic/Missions/ConditionRegistry.h"
#include "Controllers/Logic/Missions/ObjectiveRegistry.h"
#include "Models/MissionDef.h"

namespace lm {

struct CompiledMission {
	MissionDef def;
	ConditionPtr offerWhen;
	ObjectiveFactory makeObjective;
};
using CompiledMissionPtr = std::shared_ptr<const CompiledMission>;

struct MissionParseResult {
	std::vector<CompiledMissionPtr> missions;  // valid + enabled, JSON order
	std::vector<std::string> errors;           // a bad mission is skipped, the rest still load
	std::vector<std::string> warnings;
};

MissionParseResult parseMissions(const std::string& json, const ConditionRegistry& conds, const ObjectiveRegistry& objs, int defaultCooldown);

}  // namespace lm
