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

#include <fstream>
#include <sstream>
#include <string>

namespace lm::test {

inline std::string readContent(const std::string& rel) {
	std::ifstream in(std::string(LM_TEST_CONTENT_DIR) + rel);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

inline GameEvent ev(GameEventType t, int player = 0) {
	GameEvent e;
	e.type = t;
	e.player = player;
	return e;
}
inline GameEvent rolled(int player, int value) {
	GameEvent e = ev(GameEventType::DICE_ROLLED, player);
	e.value = value;
	return e;
}
inline GameEvent captured(int by, int victim) {
	GameEvent e = ev(GameEventType::TOKEN_CAPTURED, by);
	e.token = 0;
	e.victimPlayer = victim;
	e.victimToken = 0;
	return e;
}

}  // namespace lm::test
