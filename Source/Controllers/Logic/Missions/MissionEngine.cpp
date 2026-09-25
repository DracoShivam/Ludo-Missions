#include "Controllers/Logic/Missions/MissionEngine.h"

#include <algorithm>

#include "Controllers/Logic/BoardQueries.h"
#include "Controllers/Logic/Missions/Director/WeightedRandomStrategy.h"
#include "Controllers/Logic/Missions/TextTemplate.h"

namespace lm {

MissionEngine::MissionEngine() {
	registerBuiltinConditions(m_conds);  // explicit registration (no static self-registration)
	registerBuiltinObjectives(m_objs);
	m_strategy = std::make_unique<WeightedRandomStrategy>();
}

MissionParseResult MissionEngine::loadFromJson(const std::string& json) {
	auto r = parseMissions(json, m_conds, m_objs, m_settings.defaultCooldownTurns);
	m_defs = r.missions;
	return r;
}

void MissionEngine::setStrategy(std::unique_ptr<IOfferStrategy> s) {
	m_strategy = s ? std::move(s) : std::make_unique<WeightedRandomStrategy>();
}

void MissionEngine::startMatch(int selfPlayer, uint32_t seed) {
	m_rng.seed(seed);
	m_active.clear();
	m_cooldownUntil.clear();
	m_stats = OfferStats{};
	m_self = selfPlayer;
	m_selfTurnIndex = 0;
	m_offersThisTurn = 0;
	m_offersThisMoment[0] = m_offersThisMoment[1] = 0;
}

std::vector<std::string> MissionEngine::definitionIds() const {
	std::vector<std::string> ids;
	for (auto& d : m_defs) ids.push_back(d->def.id);
	return ids;
}

MissionInstance MissionEngine::snapshot(const Active& a) const {
	MissionInstance i = a.instance;
	i.progress = a.tracker.objective().progress();
	i.target = a.tracker.objective().target();
	i.turnsLeft = std::max(0, a.tracker.turnsLeft());
	i.description = renderTemplate(a.mission->def.description,
								   {{"target", i.target}, {"turns", i.turns}, {"turnsLeft", i.turnsLeft}, {"progress", i.progress}});
	return i;
}

std::vector<MissionInstance> MissionEngine::activeInstances() const {
	std::vector<MissionInstance> out;
	for (auto& a : m_active) out.push_back(snapshot(a));
	return out;
}

std::vector<MissionUpdate> MissionEngine::onEvent(const GameEvent& e, const MatchState& state) {
	std::vector<MissionUpdate> out;
	bool selfTurnStart = e.type == GameEventType::TURN_STARTED && e.player == m_self;
	if (selfTurnStart) {
		m_selfTurnIndex++;
		m_offersThisTurn = 0;
		m_offersThisMoment[0] = m_offersThisMoment[1] = 0;
	}
	EvalContext ctx{state, m_self, m_selfTurnIndex};

	if (e.type == GameEventType::MATCH_ENDED) {
		for (auto& a : m_active) out.push_back({MissionUpdate::Kind::Voided, snapshot(a)});
		m_active.clear();
		return out;
	}

	// Steps 1-3: feed trackers, then resolve (never erase inside the loop)
	std::vector<Active> keep;
	for (auto& a : m_active) {
		bool changed = false;
		TrackStatus st = a.tracker.feed(e, ctx, &changed);
		if (st == TrackStatus::Active) {
			if (changed) out.push_back({MissionUpdate::Kind::Progress, snapshot(a)});
			keep.push_back(std::move(a));
		} else {
			bool completed = st == TrackStatus::Completed;
			out.push_back({completed ? MissionUpdate::Kind::Completed : MissionUpdate::Kind::Failed, snapshot(a)});
			m_cooldownUntil[a.mission->def.id] = m_selfTurnIndex + a.mission->def.cooldownTurns + 1;
			if (m_strategy) m_strategy->onResolved(a.mission->def.id, completed);
		}
	}
	m_active = std::move(keep);

	// Step 4: offer moments
	if (selfTurnStart) {
		offerPhase(OfferMoment::TurnStart, ctx, out);
	} else if (e.type == GameEventType::DICE_ROLLED && e.player == m_self && state.current == m_self && state.phase == Phase::AwaitingMove) {
		offerPhase(OfferMoment::AfterRoll, ctx, out);
	}
	return out;
}

void MissionEngine::offerPhase(OfferMoment moment, const EvalContext& ctx, std::vector<MissionUpdate>& out) {
	int mi = (int) moment;
	int momentCap = moment == OfferMoment::TurnStart ? m_settings.offersPerTurnStart : m_settings.offersPerAfterRoll;

	// Gather the missions that could be served right now. Cooldown is the one filter worth waiving when the player
	// has nothing live: with a small catalogue every mission can be cooling down at once, and a quiet HUD is worse
	// than repeating a mission sooner than the designer intended.
	auto collect = [&](bool ignoreCooldown) {
		std::vector<CompiledMissionPtr> candidates;
		for (const auto& m : m_defs) {
			const MissionDef& d = m->def;
			if (!d.enabled) continue;
			if (std::find(d.moments.begin(), d.moments.end(), moment) == d.moments.end()) continue;
			bool active = std::any_of(m_active.begin(), m_active.end(), [&](const Active& a) { return a.mission->def.id == d.id; });
			if (active) continue;
			if (!ignoreCooldown) {
				auto cd = m_cooldownUntil.find(d.id);
				if (cd != m_cooldownUntil.end() && m_selfTurnIndex < cd->second) continue;
			}
			if (d.maxPerMatch > 0 && m_stats.offersThisMatch[d.id] >= d.maxPerMatch) continue;
			if (!m->offerWhen->eval(ctx)) continue;
			if (m->makeObjective()->alreadySatisfied(ctx)) continue;
			candidates.push_back(m);
		}
		return candidates;
	};

	for (;;) {
		// "Must offer" means the player has no live mission at all. The per-turn caps exist to stop mission spam,
		// not to enforce idle time, so they give way here.
		bool mustOffer = m_settings.alwaysOn && m_active.empty();
		if ((int) m_active.size() >= m_settings.maxActive) return;
		if (!mustOffer && (m_offersThisTurn >= m_settings.offersPerTurn || m_offersThisMoment[mi] >= momentCap)) return;

		std::vector<CompiledMissionPtr> candidates = collect(false);
		if (candidates.empty() && mustOffer) candidates = collect(true);
		if (candidates.empty()) return;

		std::optional<size_t> pick;
		if (!m_forced.empty()) {
			for (size_t i = 0; i < candidates.size(); i++)
				if (candidates[i]->def.id == m_forced) pick = i;
		}
		if (!pick) {
			m_stats.humanRaceRank = queries::raceRank(ctx.state, ctx.self);
			pick = m_strategy->choose(candidates, ctx, moment, m_stats, m_rng, mustOffer);
		}
		if (!pick) return;  // nothing is feasible from here -- genuinely nothing to ask of this player

		CompiledMissionPtr m = candidates[*pick];
		auto objective = m->makeObjective();
		// A ranged mission gets its actual target solved per offer, so the same definition reads as
		// "cut 1" on a quiet board and "cut 4" on a busy one. 0 means keep the authored number.
		int solved = m_strategy ? m_strategy->solvedTarget(*pick) : 0;
		if (solved > 0) {
			objective->setTarget(solved);
		}
		MissionTracker tracker(std::move(objective), m->def.turns);
		tracker.begin(ctx, m->def.turns);
		MissionInstance inst;
		inst.uid = m_nextUid++;
		inst.id = m->def.id;
		inst.title = m->def.title;
		inst.turns = m->def.turns;
		inst.rewardCoins = m->def.rewardCoins;
		// Resolved at OFFER time, not on completion, so the card can promise what finishing is worth.
		inst.rewardPower = m->def.rewardPower;
		if (m_rewardResolver) {
			double p = m_strategy ? m_strategy->lastProbability(*pick) : -1.0;
			auto r = m_rewardResolver(m->def, p);
			inst.rewardPower = r.powerId;
			inst.rewardPowerTitle = r.powerTitle;
			inst.rewardPowerTier = r.powerTier;
		}		m_active.push_back({m, std::move(tracker), inst});
		m_stats.offersThisMatch[m->def.id]++;
		m_stats.lastOfferedId = m->def.id;
		m_offersThisTurn++;
		m_offersThisMoment[mi]++;
		out.push_back({MissionUpdate::Kind::Offered, snapshot(m_active.back())});
	}
}

std::vector<MissionUpdate> MissionEngine::endMatch() {
	std::vector<MissionUpdate> out;
	for (auto& a : m_active) out.push_back({MissionUpdate::Kind::Voided, snapshot(a)});
	m_active.clear();
	return out;
}

}  // namespace lm
