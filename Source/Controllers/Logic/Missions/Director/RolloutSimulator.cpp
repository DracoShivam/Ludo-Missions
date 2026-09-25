#include "Controllers/Logic/Missions/Director/RolloutSimulator.h"

#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Missions/MissionTracker.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"

namespace lm {

namespace {

TrackStatus feedAll(MissionTracker& t, const std::vector<GameEvent>& evs, const MatchState& s, int self, int& selfTurn, int& turnsUsed) {
	for (const auto& e : evs) {
		if (e.type == GameEventType::TURN_STARTED && e.player == self) selfTurn++;
		if (e.type == GameEventType::TURN_ENDED && e.player == self) turnsUsed++;
		EvalContext ctx{s, self, selfTurn};
		if (t.feed(e, ctx) != TrackStatus::Active) break;
	}
	return t.status();
}

// Greedy one-step lookahead toward the mission, falling back to the bot heuristic.
MoveOption missionSeekingMove(const MatchState& s, const MissionTracker& t, int self, int selfTurn, const RulesConfig& rules,
							  const std::vector<MoveOption>& opts) {
	double best = -1e18;
	MoveOption pick = opts[0];
	int before = t.objective().progress();
	for (const auto& o : opts) {
		MatchState copy = s;
		MissionTracker tc = t;
		TurnMachine tm(copy, rules);
		auto evs = tm.move(o.token, o.value);
		int st = selfTurn, used = 0;
		TrackStatus status = feedAll(tc, evs, copy, self, st, used);
		double score = BotBrain::score(s, o) + 100.0 * (tc.objective().progress() - before);
		if (status == TrackStatus::Completed) score += 1000;
		if (status == TrackStatus::Failed) score -= 1000;
		if (score > best) {
			best = score;
			pick = o;
		}
	}
	return pick;
}

}  // namespace

bool simulateMissionOnce(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
						 Rng& rng, int* turnsUsed) {
	MatchState s = state;
	MissionTracker t(mission.makeObjective(), mission.def.turns);
	int selfTurn = selfTurnIndex;
	int used = 0;
	{
		EvalContext ctx{s, self, selfTurn};
		t.begin(ctx, mission.def.turns);
	}
	TurnMachine tm(s, rules);
	for (int cmd = 0; cmd < 400 && t.status() == TrackStatus::Active && s.phase != Phase::MatchOver; cmd++) {
		std::vector<GameEvent> evs;
		if (s.phase == Phase::AwaitingRoll) {
			evs = tm.roll(rng.dice());
		} else if (s.phase == Phase::AwaitingMove) {
			auto opts = rules::legalMoves(s, s.current);
			if (opts.empty()) break;
			MoveOption o = (s.current == self) ? missionSeekingMove(s, t, self, selfTurn, rules, opts) : BotBrain::choose(s, opts, rng);
			evs = tm.move(o.token, o.value);
		} else {
			break;
		}
		if (evs.empty()) break;
		feedAll(t, evs, s, self, selfTurn, used);
	}
	if (turnsUsed) *turnsUsed = used;
	return t.status() == TrackStatus::Completed;
}

}  // namespace lm
