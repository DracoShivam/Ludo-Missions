#include "Controllers/Logic/BotBrain.h"

#include "Controllers/Logic/BoardQueries.h"
#include "Models/BoardLayout.h"

namespace lm {

double BotBrain::score(const MatchState& s, const MoveOption& o) {
	double sc = 0;
	if (o.captures) sc += 100;
	if (o.to == FINISHED) sc += 80;
	if (o.from == IN_YARD) sc += 60;
	if (o.from <= LAST_TRACK_PROGRESS && o.to >= HOME_LANE_FIRST && o.to <= HOME_LANE_LAST) sc += 50;

	bool destOnTrack = o.to >= 0 && o.to <= LAST_TRACK_PROGRESS;
	bool destSafe = !destOnTrack || board::isSafeCell(board::globalCell(o.player, o.to));
	if (destOnTrack && destSafe) sc += 30;

	bool threatenedNow = queries::enemyBehindDistance(s, o.player, o.token, 1, 6) > 0;
	MatchState after = s;
	after.players[o.player].progress[o.token] = o.to;
	if (o.captures) {
		// remove the victim so it doesn't count as a threat afterwards
		int cell = board::globalCell(o.player, o.to);
		for (int p = 0; p < (int) after.players.size(); p++) {
			if (p == o.player) continue;
			for (int t = 0; t < 4; t++) {
				if (board::globalCell(p, after.players[p].progress[t]) == cell) after.players[p].progress[t] = IN_YARD;
			}
		}
	}
	bool threatenedAfter = queries::enemyBehindDistance(after, o.player, o.token, 1, 6) > 0;
	if (threatenedNow && !threatenedAfter) sc += 40;
	if (threatenedAfter) sc -= 50;
	sc += o.to * 0.5;
	return sc;
}

MoveOption BotBrain::choose(const MatchState& s, const std::vector<MoveOption>& options, Rng& rng) {
	double best = -1e18;
	std::vector<size_t> ties;
	for (size_t i = 0; i < options.size(); i++) {
		double sc = score(s, options[i]);
		if (sc > best + 1e-9) {
			best = sc;
			ties.clear();
			ties.push_back(i);
		} else if (sc > best - 1e-9) {
			ties.push_back(i);
		}
	}
	if (ties.empty()) {
		return MoveOption{};
	}
	return options[ties[ties.size() == 1 ? 0 : rng.range(0, (int) ties.size() - 1)]];
}

}  // namespace lm
