#include "Models/GameEvent.h"

namespace lm {

namespace {
struct TypeName {
	GameEventType type;
	const char* name;
};
const TypeName TYPE_NAMES[] = {
	{GameEventType::MATCH_STARTED, "MATCH_STARTED"},
	{GameEventType::TURN_STARTED, "TURN_STARTED"},
	{GameEventType::DICE_ROLLED, "DICE_ROLLED"},
	{GameEventType::THREE_SIXES, "THREE_SIXES"},
	{GameEventType::NO_MOVES, "NO_MOVES"},
	{GameEventType::TOKEN_MOVED, "TOKEN_MOVED"},
	{GameEventType::TOKEN_UNLOCKED, "TOKEN_UNLOCKED"},
	{GameEventType::TOKEN_ENTERED_HOME_LANE, "TOKEN_ENTERED_HOME_LANE"},
	{GameEventType::TOKEN_CAPTURED, "TOKEN_CAPTURED"},
	{GameEventType::TOKEN_FINISHED, "TOKEN_FINISHED"},
	{GameEventType::BONUS_ROLL_GRANTED, "BONUS_ROLL_GRANTED"},
	{GameEventType::POWER_USED, "POWER_USED"},
	{GameEventType::TOKEN_KICKED, "TOKEN_KICKED"},
	{GameEventType::SHIELD_EXPIRED, "SHIELD_EXPIRED"},
	{GameEventType::TURN_SKIPPED, "TURN_SKIPPED"},
	{GameEventType::TURN_ENDED, "TURN_ENDED"},
	{GameEventType::PLAYER_FINISHED, "PLAYER_FINISHED"},
	{GameEventType::MATCH_ENDED, "MATCH_ENDED"},
};
const char* FIELD_NAMES[] = {"player", "token", "from", "to", "steps", "value", "victimPlayer", "victimToken", "cell", "reason", "rank"};
}  // namespace

std::optional<int> GameEvent::field(std::string_view name) const {
	if (name == "player") return player;
	if (name == "token") return token;
	if (name == "from") return from;
	if (name == "to") return to;
	if (name == "steps") return steps;
	if (name == "value") return value;
	if (name == "victimPlayer") return victimPlayer;
	if (name == "victimToken") return victimToken;
	if (name == "cell") return cell;
	if (name == "reason") return reason;
	if (name == "rank") return rank;
	return std::nullopt;
}

bool GameEvent::isKnownField(std::string_view name) {
	for (const char* f : FIELD_NAMES) {
		if (name == f) {
			return true;
		}
	}
	return false;
}

const char* gameEventTypeName(GameEventType type) {
	for (const auto& tn : TYPE_NAMES) {
		if (tn.type == type) {
			return tn.name;
		}
	}
	return "UNKNOWN";
}

std::optional<GameEventType> gameEventTypeFromString(std::string_view name) {
	for (const auto& tn : TYPE_NAMES) {
		if (name == tn.name) {
			return tn.type;
		}
	}
	return std::nullopt;
}

}  // namespace lm
