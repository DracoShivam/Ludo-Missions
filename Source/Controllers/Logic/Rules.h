#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "Models/MatchState.h"
#include "Models/MoveOption.h"

namespace lm {
namespace rules {

// Target progress for a token at `from` moved by `value`; INVALID_PROGRESS if illegal.
int targetProgress(int from, int value);

// Opponent token captured by `mover` landing at `toProgress`: at most one {player, token}.
// Capture only if toProgress in 0..50, the global cell is not safe, and exactly ONE opponent token is on it.
std::optional<std::pair<int, int>> capturableAt(const MatchState& s, int mover, int toProgress);

// For each unique pending roll value x each token. Ordered by token, then value ascending.
std::vector<MoveOption> legalMoves(const MatchState& s, int player);

// Auto-play only when exactly one distinct token can move AND it has exactly one legal value.
std::optional<MoveOption> autoMove(const MatchState& s);

// Legal values for one token of the current player (for the roll-choice chips).
std::vector<int> legalValuesForToken(const MatchState& s, int player, int token);

int totalProgress(const PlayerState& p);  // yard counts as 0, finished as 57

}  // namespace rules
}  // namespace lm
