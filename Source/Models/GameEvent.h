#pragma once

#include <optional>
#include <string_view>

namespace lm {

enum class GameEventType {
	MATCH_STARTED,
	TURN_STARTED,
	DICE_ROLLED,
	THREE_SIXES,
	NO_MOVES,
	TOKEN_MOVED,
	TOKEN_UNLOCKED,
	TOKEN_ENTERED_HOME_LANE,
	TOKEN_CAPTURED,
	TOKEN_FINISHED,
	BONUS_ROLL_GRANTED,
	POWER_USED,
	TOKEN_KICKED,
	SHIELD_EXPIRED,
	TURN_SKIPPED,
	TURN_ENDED,
	PLAYER_FINISHED,
	MATCH_ENDED,
};

enum BonusReason { BONUS_NONE = 0, BONUS_CAPTURE = 1, BONUS_FINISH = 2 };

// TOKEN_KICKED is deliberately NOT TOKEN_CAPTURED. A kick sends a token home the same way a capture does, but a
// capture mission should not be completable by spending an item a mission gave you -- that loop feeds itself.
// Mission authors opt in to kicks explicitly.

// Flat domain event. Field meaning per type is documented in docs/PLAN.md §6.3.
struct GameEvent {
	GameEventType type = GameEventType::MATCH_STARTED;
	int player = -1;
	int token = -1;
	int from = -1;
	int to = -1;
	int steps = 0;
	int value = 0;
	int victimPlayer = -1;
	int victimToken = -1;
	int cell = -1;
	int reason = 0;
	int rank = 0;

	// Reflection for the mission DSL ("where" filters / "sum" field). nullopt for unknown names.
	std::optional<int> field(std::string_view name) const;
	static bool isKnownField(std::string_view name);
};

const char* gameEventTypeName(GameEventType type);
std::optional<GameEventType> gameEventTypeFromString(std::string_view name);

}  // namespace lm
