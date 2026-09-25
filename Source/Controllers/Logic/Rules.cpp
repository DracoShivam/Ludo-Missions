#include "Controllers/Logic/Rules.h"

#include <algorithm>
#include <set>

#include "Models/BoardLayout.h"

namespace lm {
namespace rules {

int targetProgress(int from, int value) {
	if (value < 1 || value > 6) {
		return INVALID_PROGRESS;
	}
	if (from == IN_YARD) {
		return value == 6 ? 0 : INVALID_PROGRESS;
	}
	if (from >= FINISHED) {
		return INVALID_PROGRESS;
	}
	int to = from + value;
	return to <= FINISHED ? to : INVALID_PROGRESS;
}

std::optional<std::pair<int, int>> capturableAt(const MatchState& s, int mover, int toProgress) {
	if (toProgress < 0 || toProgress > LAST_TRACK_PROGRESS) {
		return std::nullopt;
	}
	int cell = board::globalCell(mover, toProgress);
	if (board::isSafeCell(cell)) {
		return std::nullopt;
	}
	int count = 0;
	std::pair<int, int> victim{-1, -1};
	for (int p = 0; p < (int) s.players.size(); p++) {
		if (p == mover) {
			continue;
		}
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			if (board::globalCell(p, s.players[p].progress[t]) == cell) {
				count++;
				victim = {p, t};
			}
		}
	}
	if (count == 1) {
		return victim;
	}
	return std::nullopt;
}

std::vector<MoveOption> legalMoves(const MatchState& s, int player) {
	std::vector<MoveOption> out;
	if (player < 0 || player >= (int) s.players.size()) {
		return out;
	}
	std::set<int> values(s.pendingRolls.begin(), s.pendingRolls.end());
	const PlayerState& ps = s.players[player];
	for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
		for (int v : values) {
			int to = targetProgress(ps.progress[t], v);
			if (to == INVALID_PROGRESS) {
				continue;
			}
			MoveOption o;
			o.player = player;
			o.token = t;
			o.value = v;
			o.from = ps.progress[t];
			o.to = to;
			o.captures = capturableAt(s, player, to).has_value();
			out.push_back(o);
		}
	}
	return out;
}

std::optional<MoveOption> autoMove(const MatchState& s) {
	auto moves = legalMoves(s, s.current);
	if (moves.size() == 1) {
		return moves[0];
	}
	return std::nullopt;  // >1 option means either several tokens or several values
}

std::vector<int> legalValuesForToken(const MatchState& s, int player, int token) {
	std::vector<int> out;
	for (const auto& m : legalMoves(s, player)) {
		if (m.token == token) {
			out.push_back(m.value);
		}
	}
	return out;
}

int totalProgress(const PlayerState& p) {
	int sum = 0;
	for (int v : p.progress) {
		sum += (v == IN_YARD) ? 0 : (v >= FINISHED ? 57 : v + 1);
	}
	return sum;
}

}  // namespace rules
}  // namespace lm
