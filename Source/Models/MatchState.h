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
	// --- power state, all turn-scoped and all reset by newMatch() ---
	// Own turns of capture immunity remaining. Ticked down in endTurn().
	int shieldTurns = 0;
	// Added to the next value this player rolls, then cleared. Applied to the value pushed onto pendingRolls, NOT
	// to the raw die -- the six and three-sixes rules must keep reading the die, or +3 would manufacture turns.
	int diceDelta = 0;
	// Next roll is forced to this value (1..6), then cleared. 0 = no force. Applied before diceDelta.
	int forcedRoll = 0;
	// Turns this player must sit out. Consumed when endTurn() picks the next seat.
	int skipTurns = 0;
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
