#include "Controllers/Logic/ConfigParser.h"

#include "Utils/JsonUtils.h"

namespace lm {

ConfigParseResult parseGameConfig(const std::string& jsonText) {
	ConfigParseResult result;
	GameConfig& c = result.config;
	rapidjson::Document doc;
	std::string err;
	if (!json::parse(jsonText, doc, err)) {
		result.errors.push_back("game_config.json: " + err);
		return result;
	}
	if (!doc.IsObject()) {
		result.errors.push_back("game_config.json: root must be an object");
		return result;
	}

	if (auto* r = json::getObject(doc, "rules")) {
		c.rules.threeSixesForfeit = json::getBool(*r, "threeSixesForfeit", c.rules.threeSixesForfeit);
		c.rules.captureBonusRoll = json::getBool(*r, "captureBonusRoll", c.rules.captureBonusRoll);
		c.rules.finishBonusRoll = json::getBool(*r, "finishBonusRoll", c.rules.finishBonusRoll);
	}
	if (auto* p = json::getObject(doc, "players")) {
		c.players.humanColor = json::getInt(*p, "humanColor", 0);
		if (c.players.humanColor != 0) {
			result.warnings.push_back("players.humanColor: only 0 (RED) is supported in the MVP; using 0");
			c.players.humanColor = 0;
		}
		auto it = p->FindMember("names");
		if (it != p->MemberEnd() && it->value.IsArray() && it->value.Size() == 4) {
			for (rapidjson::SizeType i = 0; i < 4; i++) {
				if (it->value[i].IsString()) {
					c.players.names[i] = it->value[i].GetString();
				}
			}
		}
	}
	if (auto* t = json::getObject(doc, "timing")) {
		auto& tm = c.timing;
		tm.diceRollAnim = (float) json::getDouble(*t, "diceRollAnim", tm.diceRollAnim);
		tm.tokenStep = (float) json::getDouble(*t, "tokenStep", tm.tokenStep);
		tm.captureAnim = (float) json::getDouble(*t, "captureAnim", tm.captureAnim);
		tm.botThinkDelay = (float) json::getDouble(*t, "botThinkDelay", tm.botThinkDelay);
		tm.autoMoveDelay = (float) json::getDouble(*t, "autoMoveDelay", tm.autoMoveDelay);
		tm.turnGap = (float) json::getDouble(*t, "turnGap", tm.turnGap);
		tm.resultPopupDelay = (float) json::getDouble(*t, "resultPopupDelay", tm.resultPopupDelay);
		tm.fastBotsMultiplier = (float) json::getDouble(*t, "fastBotsMultiplier", tm.fastBotsMultiplier);
	}
	if (auto* m = json::getObject(doc, "missions")) {
		auto& ms = c.missions;
		ms.enabled = json::getBool(*m, "enabled", ms.enabled);
		ms.file = json::getString(*m, "file", ms.file);
		ms.maxActive = json::getInt(*m, "maxActive", ms.maxActive);
		ms.offersPerTurn = json::getInt(*m, "offersPerTurn", ms.offersPerTurn);
		ms.defaultCooldownTurns = json::getInt(*m, "defaultCooldownTurns", ms.defaultCooldownTurns);
		if (auto* opm = json::getObject(*m, "offersPerMoment")) {
			ms.offersPerTurnStart = json::getInt(*opm, "turnStart", ms.offersPerTurnStart);
			ms.offersPerAfterRoll = json::getInt(*opm, "afterRoll", ms.offersPerAfterRoll);
		}
	}
	if (auto* d = json::getObject(doc, "director")) {
		auto& dc = c.director;
		dc.enabled = json::getBool(*d, "enabled", dc.enabled);
		dc.astarMaxExpansions = json::getInt(*d, "astarMaxExpansions", dc.astarMaxExpansions);
		dc.rollouts = json::getInt(*d, "rollouts", dc.rollouts);
		dc.simBudgetMs = json::getInt(*d, "simBudgetMs", dc.simBudgetMs);
		if (auto* df = json::getObject(*d, "difficulty")) {
			auto& x = dc.difficulty;
			x.startCenter = json::getDouble(*df, "startCenter", x.startCenter);
			x.halfWidth = json::getDouble(*df, "halfWidth", x.halfWidth);
			x.stepOnComplete = json::getDouble(*df, "stepOnComplete", x.stepOnComplete);
			x.stepOnFail = json::getDouble(*df, "stepOnFail", x.stepOnFail);
			x.min = json::getDouble(*df, "min", x.min);
			x.max = json::getDouble(*df, "max", x.max);
			x.behindBias = json::getDouble(*df, "behindBias", x.behindBias);
			x.persist = json::getBool(*df, "persist", x.persist);
		}
		if (auto* u = json::getObject(*d, "utility")) {
			auto& x = dc.utility;
			x.timelyBonus = json::getDouble(*u, "timelyBonus", x.timelyBonus);
			x.noveltyPower = json::getDouble(*u, "noveltyPower", x.noveltyPower);
			x.repeatPenalty = json::getDouble(*u, "repeatPenalty", x.repeatPenalty);
			x.temperature = json::getDouble(*u, "temperature", x.temperature);
			x.minUtility = json::getDouble(*u, "minUtility", x.minUtility);
			x.fitFloor = json::getDouble(*u, "fitFloor", x.fitFloor);
		}
	}
	if (auto* g = json::getObject(doc, "debug")) {
		auto& dbg = c.debug;
		dbg.rngSeed = (unsigned int) json::getInt(*g, "rngSeed", 0);
		dbg.fastBots = json::getBool(*g, "fastBots", dbg.fastBots);
		dbg.forcedMission = json::getString(*g, "forcedMission", "");
		auto it = g->FindMember("startProgress");
		if (it != g->MemberEnd() && it->value.IsArray() && it->value.Size() > 0) {
			bool ok = it->value.Size() == 4;
			std::vector<std::vector<int>> sp;
			for (rapidjson::SizeType i = 0; ok && i < it->value.Size(); i++) {
				const auto& row = it->value[i];
				if (!row.IsArray() || row.Size() != 4) {
					ok = false;
					break;
				}
				std::vector<int> r;
				for (rapidjson::SizeType j = 0; j < 4; j++) {
					int v = row[j].IsInt() ? row[j].GetInt() : -1;
					if (v < -1 || v > 56) {
						ok = false;
					}
					r.push_back(v);
				}
				sp.push_back(r);
			}
			if (ok) {
				dbg.startProgress = sp;
			} else {
				result.errors.push_back("debug.startProgress must be a 4x4 array of values in -1..56; ignored");
			}
		}
	}
	return result;
}

}  // namespace lm
