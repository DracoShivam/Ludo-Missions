#pragma once

#include "Models/MatchState.h"

namespace lm {
namespace queries {

enum class Zone { Yard, Track, HomeLane, Finished, OutOfYard };

// Smallest d in [minD, maxD] such that `token` (on track, progress+d <= 50) would land on a non-safe cell holding
// exactly one enemy token. 0 if none.
int enemyAheadDistance(const MatchState& s, int player, int token, int minD, int maxD);
// Smallest d in [minD, maxD] such that an enemy token on the track (its progress+d <= 50) sits d cells behind `token`,
// and `token` is on a non-safe track cell. 0 if none.
int enemyBehindDistance(const MatchState& s, int player, int token, int minD, int maxD);

bool anyEnemyAhead(const MatchState& s, int player, int minD, int maxD);
bool anyEnemyBehind(const MatchState& s, int player, int minD, int maxD);

int countTokens(const MatchState& s, int player, Zone zone);
int countEnemyTokens(const MatchState& s, int player, Zone zone);
int maxProgress(const MatchState& s, int player, bool excludeFinished);
// 1 = leading .. 4 = last, by rules::totalProgress (ties share the better rank)
int raceRank(const MatchState& s, int player);

}  // namespace queries
}  // namespace lm
