#include "Controllers/Logic/BoardQueries.h"

#include <algorithm>

#include "Controllers/Logic/Rules.h"
#include "Models/BoardLayout.h"

namespace lm {
namespace queries {

namespace {
int enemyCountAtCell(const MatchState& s, int player, int cell) {
	int count = 0;
	for (int p = 0; p < (int) s.players.size(); p++) {
		if (p == player) {
			continue;
		}
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			if (board::globalCell(p, s.players[p].progress[t]) == cell) {
				count++;
			}
		}
	}
	return count;
}
}  // namespace

int enemyAheadDistance(const MatchState& s, int player, int token, int minD, int maxD) {
	int prog = s.players[player].progress[token];
	if (prog < 0 || prog > LAST_TRACK_PROGRESS) {
		return 0;
	}
	for (int d = std::max(1, minD); d <= maxD; d++) {
		if (prog + d > LAST_TRACK_PROGRESS) {
			break;
		}
		int cell = board::globalCell(player, prog + d);
		if (!board::isSafeCell(cell) && enemyCountAtCell(s, player, cell) == 1) {
			return d;
		}
	}
	return 0;
}

int enemyBehindDistance(const MatchState& s, int player, int token, int minD, int maxD) {
	int prog = s.players[player].progress[token];
	if (prog < 0 || prog > LAST_TRACK_PROGRESS) {
		return 0;
	}
	int myCell = board::globalCell(player, prog);
	if (board::isSafeCell(myCell)) {
		return 0;
	}
	for (int d = std::max(1, minD); d <= maxD; d++) {
		int cell = ((myCell - d) % TRACK_LEN + TRACK_LEN) % TRACK_LEN;
		for (int p = 0; p < (int) s.players.size(); p++) {
			if (p == player) {
				continue;
			}
			for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
				int pe = s.players[p].progress[t];
				if (pe >= 0 && pe + d <= LAST_TRACK_PROGRESS && board::globalCell(p, pe) == cell) {
					return d;
				}
			}
		}
	}
	return 0;
}

bool anyEnemyAhead(const MatchState& s, int player, int minD, int maxD) {
	for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
		if (enemyAheadDistance(s, player, t, minD, maxD) > 0) {
			return true;
		}
	}
	return false;
}

bool anyEnemyBehind(const MatchState& s, int player, int minD, int maxD) {
	for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
		if (enemyBehindDistance(s, player, t, minD, maxD) > 0) {
			return true;
		}
	}
	return false;
}

static bool inZone(int prog, Zone zone) {
	switch (zone) {
		case Zone::Yard:
			return prog == IN_YARD;
		case Zone::Track:
			return prog >= 0 && prog <= LAST_TRACK_PROGRESS;
		case Zone::HomeLane:
			return prog >= HOME_LANE_FIRST && prog <= HOME_LANE_LAST;
		case Zone::Finished:
			return prog >= FINISHED;
		case Zone::OutOfYard:
			return prog >= 0;
	}
	return false;
}

int countTokens(const MatchState& s, int player, Zone zone) {
	int n = 0;
	for (int v : s.players[player].progress) {
		n += inZone(v, zone) ? 1 : 0;
	}
	return n;
}

int countEnemyTokens(const MatchState& s, int player, Zone zone) {
	int n = 0;
	for (int p = 0; p < (int) s.players.size(); p++) {
		if (p != player) {
			n += countTokens(s, p, zone);
		}
	}
	return n;
}

int maxProgress(const MatchState& s, int player, bool excludeFinished) {
	int best = IN_YARD;
	for (int v : s.players[player].progress) {
		if (excludeFinished && v >= FINISHED) {
			continue;
		}
		best = std::max(best, v);
	}
	return best;
}

int raceRank(const MatchState& s, int player) {
	int mine = rules::totalProgress(s.players[player]);
	int rank = 1;
	for (int p = 0; p < (int) s.players.size(); p++) {
		if (p != player && rules::totalProgress(s.players[p]) > mine) {
			rank++;
		}
	}
	return rank;
}

}  // namespace queries
}  // namespace lm
