#pragma once

#include <string>
#include <vector>

#include "Models/GameConfig.h"
#include "Models/GameEvent.h"
#include "Models/MatchState.h"
#include "Models/MoveOption.h"

// Controller -> View (and -> MissionController) messages for the Ludo game.
namespace lm {

// Single channel for all domain events (docs/PLAN.md §6.3). Views multiply animation durations by animScale.
struct GameEventMsg {
	static constexpr const char* NAME = "lm.game.event";
	GameEvent event;
	float animScale = 1.0f;
};

struct MatchSnapshot {
	static constexpr const char* NAME = "lm.game.snapshot";
	MatchState state;
	TimingConfig timing;
	std::vector<std::string> names;
};

// Final standings, published immediately BEFORE the MATCH_ENDED GameEventMsg.
struct MatchRanking {
	static constexpr const char* NAME = "lm.game.ranking";
	std::vector<int> ranking;  // player indices, winner first
};

struct AwaitingRollMsg {
	static constexpr const char* NAME = "lm.game.awaitingRoll";
	int player = -1;
	bool isHuman = false;
};

struct AwaitingMoveMsg {
	static constexpr const char* NAME = "lm.game.awaitingMove";
	int player = -1;
	bool isHuman = false;
	std::vector<MoveOption> options;
};

// One token on the board.
struct TokenRef {
	int player = -1;
	int token = -1;
	bool operator==(const TokenRef& o) const { return player == o.player && token == o.token; }
};

// THE single source of truth for what the player may tap. Both the move flow and the
// power-targeting flow publish it, and BoardView neither knows nor cares which sent it.
//
// This message exists because "tappable" and "has a legal dice move" used to be the same thing:
// BoardView only hit-tested highlighted tokens, highlights only ever came from the human's legal
// moves, and so an enemy token could never be tapped at all -- which made every power that needs
// a target silently impossible to use.
struct TappableTokens {
	static constexpr const char* NAME = "lm.game.tappableTokens";
	enum class Reason { Move, PowerTarget };
	Reason reason = Reason::Move;
	std::vector<TokenRef> tokens;
};

struct RollChoiceRequested {
	static constexpr const char* NAME = "lm.game.rollChoiceRequested";
	int player = -1;
	int token = -1;
	std::vector<int> values;
};

struct PendingRollsChanged {
	static constexpr const char* NAME = "lm.game.pendingRollsChanged";
	int player = -1;
	std::vector<int> rolls;
};

}  // namespace lm
