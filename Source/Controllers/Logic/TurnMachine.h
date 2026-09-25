#pragma once

#include <vector>

#include "Models/GameConfig.h"
#include "Models/GameEvent.h"
#include "Models/MatchState.h"

namespace lm {

// Pure Ludo turn state machine (STACK_AND_MOVE sixes). Mutates the MatchState it is given and returns
// the ordered domain events produced by each command. It never advances on its own: the caller decides
// when to roll / move (GameController adds pacing; tests and the Mission Director call it directly).
class TurnMachine {
public:
	TurnMachine(MatchState& state, const RulesConfig& rules);

	std::vector<GameEvent> startMatch();
	std::vector<GameEvent> roll(int value);             // precondition: phase == AwaitingRoll, 1..6
	std::vector<GameEvent> move(int token, int value);  // precondition: phase == AwaitingMove and legal
	// Give up the rest of the turn. Used when spending a power is configured to cost the turn; the normal
	// paths end turns on their own.
	std::vector<GameEvent> endTurnNow();

private:
	void endTurn(std::vector<GameEvent>& out);
	void endMatch(int winner, std::vector<GameEvent>& out);
	void resolveMovesOrEndTurn(std::vector<GameEvent>& out);
	GameEvent ev(GameEventType type) const;

	MatchState& m_state;
	RulesConfig m_rules;
};

}  // namespace lm
