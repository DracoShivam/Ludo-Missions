#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Controllers/Logic/Missions/ConditionRegistry.h"
#include "Controllers/Logic/Missions/Director/IOfferStrategy.h"
#include "Controllers/Logic/Missions/MissionParser.h"
#include "Controllers/Logic/Missions/MissionTracker.h"
#include "Controllers/Logic/Missions/ObjectiveRegistry.h"
#include "Controllers/Logic/Rng.h"
#include "Models/GameConfig.h"
#include "Models/MissionInstance.h"

namespace lm {

// Pure mission runtime (docs/PLAN.md §7.5). Feed it every GameEvent (in order) with the current MatchState.
class MissionEngine {
public:
	MissionEngine();
	MissionParseResult loadFromJson(const std::string& json);  // replaces defs for FUTURE offers
	void setSettings(const MissionSettings& s) { m_settings = s; }
	void setStrategy(std::unique_ptr<IOfferStrategy> s);
	IOfferStrategy* strategy() const { return m_strategy.get(); }
	void startMatch(int selfPlayer, uint32_t seed);
	std::vector<MissionUpdate> onEvent(const GameEvent& e, const MatchState& state);
	std::vector<MissionUpdate> endMatch();  // Voided for each active
	std::vector<MissionInstance> activeInstances() const;
	void setForcedMission(const std::string& id) { m_forced = id; }
	std::vector<std::string> definitionIds() const;
	const std::vector<CompiledMissionPtr>& definitions() const { return m_defs; }
	int selfTurnIndex() const { return m_selfTurnIndex; }
	const ConditionRegistry& conditions() const { return m_conds; }
	const ObjectiveRegistry& objectives() const { return m_objs; }

private:
	struct Active {
		CompiledMissionPtr mission;  // keeps the def alive across hot reloads
		MissionTracker tracker;
		MissionInstance instance;
	};
	MissionInstance snapshot(const Active& a) const;
	void offerPhase(OfferMoment moment, const EvalContext& ctx, std::vector<MissionUpdate>& out);

	ConditionRegistry m_conds;
	ObjectiveRegistry m_objs;
	std::vector<CompiledMissionPtr> m_defs;
	MissionSettings m_settings;
	std::unique_ptr<IOfferStrategy> m_strategy;
	Rng m_rng;
	std::vector<Active> m_active;
	std::map<std::string, int> m_cooldownUntil;
	OfferStats m_stats;
	int m_self = 0;
	int m_selfTurnIndex = 0;
	int m_offersThisTurn = 0;
	int m_offersThisMoment[2] = {0, 0};
	int m_nextUid = 1;
	std::string m_forced;
};

}  // namespace lm
