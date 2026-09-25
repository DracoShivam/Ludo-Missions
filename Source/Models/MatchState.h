#pragma once

#include <array>
#include <vector>

#include "Models/Types.h"

namespace lm {

struct PlayerState {
	int color = 0;
	PlayerKind kind = PlayerKind::Bot;
	std::array<int, TOKENS_PER_PLAYER> progress{IN_YARD, IN_YARD, IN_YARD, IN_YARD};
	int finishRank = 0;  // 0 = not ranked yet
	// endTurn() skips this player like a finished one. ONLY used by the Mission Director's A* search to freeze bots.
	bool sitsOut = false;
};

// NO strings / maps here: the Mission Director copies MatchState thousands of times per decision.
struct MatchState {
	std::vector<PlayerState> players;  // index == color
	int selfPlayer = 0;                // the human
	int current = 0;                   // whose turn
	Phase phase = Phase::NotStarted;
	std::vector<int> pendingRolls;  // STACK_AND_MOVE stack, in roll order
	int consecutiveSixes = 0;
	bool bonusRollPending = false;
	int turnNumber = 0;        // increments on every TURN_STARTED (all players)
	std::vector<int> ranking;  // player indices, best first; filled at MatchOver
};

}  // namespace lm
