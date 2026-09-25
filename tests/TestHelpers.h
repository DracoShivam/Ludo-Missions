#pragma once

#include <array>
#include <vector>

#include "Models/GameEvent.h"
#include "Models/MatchState.h"

namespace lm::test {

// progress[player][token]; player 0 is the human.
inline MatchState makeState(std::array<std::array<int, 4>, 4> progress, int current = 0) {
	MatchState s;
	for (int p = 0; p < 4; p++) {
		PlayerState ps;
		ps.color = p;
		ps.kind = p == 0 ? PlayerKind::Human : PlayerKind::Bot;
		ps.progress = progress[p];
		s.players.push_back(ps);
	}
	s.selfPlayer = 0;
	s.current = current;
	s.phase = Phase::AwaitingRoll;
	s.turnNumber = 1;
	return s;
}

inline MatchState allInYard() {
	return makeState({{{-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
}

inline std::vector<GameEventType> types(const std::vector<GameEvent>& evs) {
	std::vector<GameEventType> out;
	for (const auto& e : evs) out.push_back(e.type);
	return out;
}

inline bool contains(const std::vector<GameEvent>& evs, GameEventType t) {
	for (const auto& e : evs)
		if (e.type == t) return true;
	return false;
}

}  // namespace lm::test
