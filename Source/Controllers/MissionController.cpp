#include "Controllers/MissionController.h"

#include <algorithm>

#include "axmol.h"
#include "Controllers/ConfigController.h"
#include "Controllers/GameController.h"
#include "Controllers/Logic/Missions/Director/DirectorStrategy.h"
#include "Controllers/PowerController.h"
#include "Controllers/WalletController.h"
#include "Events/AppEvents.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Events/MissionEvents.h"
#include "Events/UiEvents.h"
#include "Utils/Log.h"

namespace lm {

static const char* DIFFICULTY_KEY = "lm.director.center";

MissionController* MissionController::sharedController() {
	static MissionController* s_instance = new MissionController();
	return s_instance;
}

void MissionController::init() {
	const GameConfig& cfg = ConfigController::sharedController()->config();
	if (!cfg.missions.enabled) {
		LM_LOG("missions disabled (missions.enabled=false)");
		return;  // modularity: subscribe to nothing
	}
	m_forced = cfg.debug.forcedMission;
	loadMissions();

	EventBus::subscribe<GameEventMsg>(this, [this](const GameEventMsg& m) {
		const GameEvent& e = m.event;
		const MatchState& state = GameController::sharedController()->matchState();
		if (e.type == GameEventType::MATCH_STARTED) {
			const GameConfig& c = ConfigController::sharedController()->config();
			uint32_t seed = c.debug.rngSeed ? c.debug.rngSeed + 1 : 0;
			m_engine.startMatch(state.selfPlayer, seed);
			m_rng.seed(seed ? seed + 2 : 0);  // own stream: power drops must not perturb the director's
			m_completed = m_failed = m_coins = 0;
			m_matchActive = true;
			return;
		}
		if (!m_matchActive) return;
		if (e.type == GameEventType::MATCH_ENDED) {
			MissionMatchSummary s;
			s.completed = m_completed;
			s.failed = m_failed;
			s.coinsEarned = m_coins;
			EventBus::publish(s);
			m_matchActive = false;
		}
		auto updates = m_engine.onEvent(e, state);
		if (m_director && !updates.empty() && !m_director->lastDecisionLog().empty()) {
			for (auto& u : updates) {
				if (u.kind == MissionUpdate::Kind::Offered) LM_LOG("[Director] %s", m_director->lastDecisionLog().c_str());
			}
		}
		publish(updates);
	});
	EventBus::subscribe<UiGameSceneExiting>(this, [this](const UiGameSceneExiting&) {
		if (!m_matchActive) return;
		m_matchActive = false;
		publish(m_engine.endMatch());
	});
	EventBus::subscribe<DebugReloadConfig>(this, [this](const DebugReloadConfig&) { loadMissions(); });
	EventBus::subscribe<DebugCycleForcedMission>(this, [this](const DebugCycleForcedMission&) {
		auto ids = m_engine.definitionIds();
		auto it = std::find(ids.begin(), ids.end(), m_forced);
		m_forced = (it == ids.end()) ? (ids.empty() ? "" : ids[0]) : (it + 1 == ids.end() ? "" : *(it + 1));
		m_engine.setForcedMission(m_forced);
		DebugStateChanged d;
		d.forcedMission = m_forced;
		d.hasForcedMission = true;
		EventBus::publish(d);
		LM_LOG("forced mission: %s", m_forced.empty() ? "(none)" : m_forced.c_str());
	});
	EventBus::subscribe<DebugResetCoins>(this, [this](const DebugResetCoins&) {
		if (m_director) {
			m_director->difficulty().reset();
			saveDifficulty();
		}
	});
}

void MissionController::loadMissions() {
	const GameConfig& cfg = ConfigController::sharedController()->config();
	m_engine.setSettings(cfg.missions);
	rebuildStrategy();

	// A mission's power reward is resolved when it is OFFERED, not when it completes, so the card
	// can promise it. The tier comes from the Director's own measured completion probability, which
	// means retuning a mission moves its reward automatically -- there is no second number to keep
	// in sync. The engine never learns what a power is; it receives an id and carries it.
	m_engine.setRewardResolver([](const MissionDef& def, double pComplete) -> MissionEngine::ResolvedReward {
		MissionEngine::ResolvedReward out;
		const GameConfig& c = ConfigController::sharedController()->config();
		if (!c.powers.enabled) {
			return out;
		}
		auto* pc = PowerController::sharedController();
		// A designer may pin a power on a mission; otherwise the tier is measured, not authored.
		out.powerId = def.rewardPower.empty() ? pc->drawId(pc->tierForProbability(pComplete)) : def.rewardPower;
		if (const CompiledPower* cp = pc->catalog().find(out.powerId)) {
			out.powerTitle = cp->def.title;
			out.powerTier = (int) cp->def.tier;
		} else {
			out.powerId.clear();
		}
		return out;
	});
	std::string text = ConfigController::readText(cfg.missions.file);
	auto r = m_engine.loadFromJson(text);
	for (auto& e : r.errors) LM_LOG_ERROR("%s", e.c_str());
	for (auto& w : r.warnings) LM_LOG_WARN("%s", w.c_str());
	LM_LOG("missions loaded: %d (%d errors, %d warnings)", (int) r.missions.size(), (int) r.errors.size(), (int) r.warnings.size());
	m_engine.setForcedMission(m_forced);
	MissionsReloaded ev;
	ev.count = (int) r.missions.size();
	ev.errors = (int) r.errors.size();
	ev.warnings = (int) r.warnings.size();
	EventBus::publish(ev);
}

void MissionController::rebuildStrategy() {
	const GameConfig& cfg = ConfigController::sharedController()->config();
	double center = m_director ? m_director->difficulty().center() : -1;
	if (cfg.director.enabled) {
		auto d = std::make_unique<DirectorStrategy>(cfg.director, cfg.rules);
		m_director = d.get();
		if (center >= 0) {
			m_director->difficulty().setCenter(center);
		} else if (cfg.director.difficulty.persist) {
			float saved = ax::UserDefault::getInstance()->getFloatForKey(DIFFICULTY_KEY, -1.f);
			if (saved >= 0) m_director->difficulty().setCenter(saved);
		}
		m_engine.setStrategy(std::move(d));
	} else {
		m_director = nullptr;
		m_engine.setStrategy(nullptr);  // weighted random fallback
	}
}

void MissionController::saveDifficulty() {
	if (!m_director || !ConfigController::sharedController()->config().director.difficulty.persist) return;
	ax::UserDefault::getInstance()->setFloatForKey(DIFFICULTY_KEY, (float) m_director->difficulty().center());
	ax::UserDefault::getInstance()->flush();
}

void MissionController::publish(const std::vector<MissionUpdate>& updates) {
	for (const auto& u : updates) {
		LM_LOG("mission %s: %s (%d/%d, %d turns left)", missionUpdateKindName(u.kind), u.instance.id.c_str(), u.instance.progress, u.instance.target,
			   u.instance.turnsLeft);
		if (u.kind == MissionUpdate::Kind::Completed) {
			m_completed++;
			m_coins += u.instance.rewardCoins;
			WalletController::sharedController()->add(u.instance.rewardCoins, "mission:" + u.instance.id);
		if (!u.instance.rewardPower.empty()) {
			PowerController::sharedController()->grant(GameController::sharedController()->matchState().selfPlayer,
													   u.instance.rewardPower, "mission:" + u.instance.id);
		}
			saveDifficulty();
		} else if (u.kind == MissionUpdate::Kind::Failed) {
			m_failed++;
			saveDifficulty();
		}
		MissionUpdated ev;
		ev.update = u;
		EventBus::publish(ev);
	}
}

}  // namespace lm
