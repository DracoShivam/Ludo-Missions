#pragma once

namespace lm {

constexpr int NUM_PLAYERS = 4;
constexpr int TOKENS_PER_PLAYER = 4;

// Colour index == player index. RED bottom-left, GREEN top-left, YELLOW top-right, BLUE bottom-right.
enum PlayerColor { RED = 0, GREEN = 1, YELLOW = 2, BLUE = 3 };

enum class PlayerKind { Human, Bot };

enum class Phase { NotStarted, AwaitingRoll, AwaitingMove, MatchOver };

// Token progress encoding (relative to the token's own colour).
constexpr int IN_YARD = -1;
constexpr int LAST_TRACK_PROGRESS = 50;  // 0..50 = on the shared track
constexpr int HOME_LANE_FIRST = 51;      // 51..55 = home lane
constexpr int HOME_LANE_LAST = 55;
constexpr int FINISHED = 56;
constexpr int TRACK_LEN = 52;
constexpr int INVALID_PROGRESS = -999;

}  // namespace lm
