#include "Controllers/Logic/Missions/Director/FeasibilitySearch.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

#include "Controllers/Logic/Missions/MissionTracker.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"

namespace lm {

namespace {

struct Node {
	MatchState s;
	MissionTracker t;
	int g;
	int selfTurn;
};

struct HeapItem {
	int f, g;
	uint64_t seq;
	size_t idx;
	bool operator>(const HeapItem& o) const {
		if (f != o.f) return f > o.f;
		if (g != o.g) return g > o.g;
		return seq > o.seq;
	}
};

inline void mix(uint64_t& h, uint64_t v) {
	h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
}

uint64_t stateHash(const Node& n) {
	uint64_t h = 1469598103934665603ULL;
	for (const auto& p : n.s.players)
		for (int v : p.progress) mix(h, (uint64_t) (v + 2));
	std::vector<int> rolls = n.s.pendingRolls;
	std::sort(rolls.begin(), rolls.end());
	for (int r : rolls) mix(h, (uint64_t) r * 131);
	mix(h, (uint64_t) n.s.consecutiveSixes);
	mix(h, n.s.bonusRollPending ? 7 : 3);
	mix(h, (uint64_t) n.s.phase);
	mix(h, n.t.objective().stateKey());
	mix(h, (uint64_t) (n.t.turnsLeft() + 100));
	return h;
}

}  // namespace

SearchResult feasibilitySearch(const CompiledMission& mission, const MatchState& state, int self, int selfTurnIndex, const RulesConfig& rules,
							   int maxExpansions) {
	SearchResult res;
	if (state.current != self || (state.phase != Phase::AwaitingRoll && state.phase != Phase::AwaitingMove)) {
		return res;  // Unknown
	}
	Node root{state, MissionTracker(mission.makeObjective(), mission.def.turns), 0, selfTurnIndex};
	for (int p = 0; p < (int) root.s.players.size(); p++) {
		if (p != self) root.s.players[p].sitsOut = true;
	}
	{
		EvalContext ctx{root.s, self, selfTurnIndex};
		root.t.begin(ctx, mission.def.turns);
	}
	const int turns = mission.def.turns;
	if (root.t.objective().completesWhenBotsFrozen()) {
		res.verdict = SearchResult::Verdict::Feasible;  // cannot fail while bots are frozen
		res.minTurns = turns;
		return res;
	}

	std::vector<Node> nodes;
	nodes.reserve(1024);
	std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> open;
	std::unordered_map<uint64_t, int> bestG;
	uint64_t seq = 0;

	auto push = [&](Node&& n) {
		uint64_t key = stateHash(n);
		auto it = bestG.find(key);
		if (it != bestG.end() && it->second <= n.g) return;
		bestG[key] = n.g;
		EvalContext ctx{n.s, self, n.selfTurn};
		int h = n.t.status() == TrackStatus::Completed ? 0 : n.t.objective().minTurnsHint(ctx, n.t.turnsLeft());
		if (n.g + h > turns) return;  // admissible bound: cannot finish inside the window
		nodes.push_back(std::move(n));
		open.push({nodes.back().g + h, nodes.back().g, seq++, nodes.size() - 1});
	};
	push(std::move(root));

	while (!open.empty()) {
		HeapItem top = open.top();
		open.pop();
		// copy out: `nodes` may reallocate while we push children
		Node cur = nodes[top.idx];
		if (cur.t.status() == TrackStatus::Completed) {
			res.verdict = SearchResult::Verdict::Feasible;
			res.minTurns = cur.g;
			return res;
		}
		if (res.expansions >= maxExpansions) {
			res.verdict = SearchResult::Verdict::Unknown;
			res.minTurns = top.f;
			return res;
		}
		res.expansions++;

		auto expand = [&](auto command) {
			Node child = cur;
			TurnMachine tm(child.s, rules);
			std::vector<GameEvent> evs = command(tm);
			if (evs.empty()) return;
			for (const auto& e : evs) {
				if (e.type == GameEventType::TURN_STARTED && e.player == self) child.selfTurn++;
				if (e.type == GameEventType::TURN_ENDED && e.player == self) child.g++;  // counted even if it resolves the mission
				EvalContext ctx{child.s, self, child.selfTurn};
				if (child.t.feed(e, ctx) != TrackStatus::Active) break;
			}
			if (child.t.status() == TrackStatus::Failed) return;
			if (child.t.status() == TrackStatus::Active && (child.s.phase == Phase::MatchOver || child.g > turns)) return;
			push(std::move(child));
		};

		if (cur.s.phase == Phase::AwaitingRoll) {
			for (int v = 1; v <= 6; v++) expand([v](TurnMachine& tm) { return tm.roll(v); });
		} else if (cur.s.phase == Phase::AwaitingMove) {
			for (const auto& o : rules::legalMoves(cur.s, cur.s.current)) {
				int token = o.token, value = o.value;
				expand([token, value](TurnMachine& tm) { return tm.move(token, value); });
			}
		}
	}
	res.verdict = SearchResult::Verdict::Infeasible;
	return res;
}

}  // namespace lm
