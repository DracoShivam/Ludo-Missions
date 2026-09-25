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
