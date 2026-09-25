// A headless soak: many seeded matches in which every player spends powers at random, with the
// game's invariants checked after EVERY command.
//
// Watching autoplay can only show what happens to be on screen when it goes wrong. This drives
// thousands of turns a second and asserts the things a human eye cannot: that no token ever holds
// an impossible position, that a modified roll stays spendable, that a move is always accompanied
// by an event the board could draw, and that a match with powers in it still terminates.

#include "doctest/doctest.h"

#include <algorithm>

#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Powers/PowerCatalog.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "TestHelpers.h"

using namespace lm;

namespace {

struct Inventory {
	std::vector<std::string> held;
};

// Everything that must be true of a MatchState at all times, powers or no powers.
void checkInvariants(const MatchState& s, const char* where) {
	for (int p = 0; p < (int) s.players.size(); p++) {
		const auto& pl = s.players[p];
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			int prog = pl.progress[t];
			CHECK_MESSAGE(prog >= IN_YARD, where << ": seat " << p << " token " << t << " progress " << prog);
			CHECK_MESSAGE(prog <= FINISHED, where << ": seat " << p << " token " << t << " progress " << prog);
		}
		CHECK_MESSAGE(pl.shieldTurns >= 0, where << ": negative shield");
		CHECK_MESSAGE(pl.skipTurns >= 0, where << ": negative skip");
		CHECK_MESSAGE(pl.forcedRoll >= 0, where << ": negative forced roll");
		CHECK_MESSAGE(pl.forcedRoll <= 6, where << ": forced roll above a die face");
	}
	for (int v : s.pendingRolls) {
		CHECK_MESSAGE(v >= 1, where << ": pending roll " << v << " is not spendable");
		CHECK_MESSAGE(v <= MAX_ROLL_VALUE, where << ": pending roll " << v << " exceeds MAX_ROLL_VALUE");
	}
	CHECK_MESSAGE(s.current >= 0, where << ": no current player");
	CHECK_MESSAGE(s.current < (int) s.players.size(), where << ": current player out of range");
}

// Every token whose position changed must be named by an event the board can draw, or it is
// invisible on screen -- the exact defect that made Send Home and Swap look broken.
void checkMovesWereReported(const MatchState& before, const MatchState& after, const std::vector<GameEvent>& evs,
							const std::string& what) {
	for (int p = 0; p < (int) after.players.size(); p++) {
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			if (before.players[p].progress[t] == after.players[p].progress[t]) continue;
			bool named = false;
			for (const auto& e : evs) {
				bool renderable = e.type == GameEventType::TOKEN_MOVED || e.type == GameEventType::TOKEN_CAPTURED ||
								  e.type == GameEventType::TOKEN_KICKED;
				if (!renderable) continue;
				named = named || (e.player == p && e.token == t) || (e.victimPlayer == p && e.victimToken == t);
			}
			CHECK_MESSAGE(named, what << ": seat " << p << " token " << t << " moved with no event the board can draw");
		}
	}
}

}  // namespace

TEST_CASE("soak: matches with powers terminate and never corrupt the board") {
	PowerCatalog cat;
	REQUIRE(cat.loadFromJson(test::readContent("config/powers.json")).errors.empty());

	int matchesFinished = 0, powersFired = 0, totalTurns = 0;

	for (uint32_t seed = 1; seed <= 60; seed++) {
		MatchState s = test::allInYard();
		RulesConfig rules;
		TurnMachine tm(s, rules);
		Rng rng(seed);
		std::vector<Inventory> inv(s.players.size());

		int cmd = 0;
		for (; cmd < 6000 && s.phase != Phase::MatchOver; cmd++) {
			int seat = s.current;

			// Hand out a power now and then, to whoever is up.
			if (rng.range(1, 6) == 1 && inv[seat].held.size() < 3) {
				if (const CompiledPower* p = cat.draw((PowerTier) rng.range(0, 2), rng)) {
					inv[seat].held.push_back(p->def.id);
				}
			}

			// ...and spend one at random when it is legal to.
			if (!inv[seat].held.empty() && rng.range(1, 3) == 1) {
				int idx = rng.range(0, (int) inv[seat].held.size() - 1);
				const CompiledPower* p = cat.find(inv[seat].held[idx]);
				REQUIRE(p != nullptr);
				auto targets = p->effect->targets({s, seat});
				if (!targets.empty()) {
					PowerTarget target = targets[rng.range(0, (int) targets.size() - 1)];
					MatchState before = s;
					auto evs = p->effect->apply(s, seat, target);
					CHECK_MESSAGE(!evs.empty(), "'" << p->def.id << "' accepted a target it offered, then did nothing");
					checkMovesWereReported(before, s, evs, p->def.id);
					checkInvariants(s, p->def.id.c_str());
					inv[seat].held.erase(inv[seat].held.begin() + idx);
					powersFired++;
				}
			}

			if (s.phase == Phase::AwaitingRoll) {
				MatchState before = s;
				auto evs = tm.roll(rng.dice());
				checkMovesWereReported(before, s, evs, "roll");
				checkInvariants(s, "after roll");
			} else if (s.phase == Phase::AwaitingMove) {
				auto opts = rules::legalMoves(s, s.current);
				// A power must never leave the player holding rolls they cannot spend.
				REQUIRE_MESSAGE(!opts.empty(), "seed " << seed << ": AwaitingMove with no legal move");
				MoveOption o = BotBrain::choose(s, opts, rng);
				MatchState before = s;
				auto evs = tm.move(o.token, o.value);
				REQUIRE_MESSAGE(!evs.empty(), "seed " << seed << ": a legal move was rejected");
				checkMovesWereReported(before, s, evs, "move");
				checkInvariants(s, "after move");
			} else {
				break;
			}
		}

		CHECK_MESSAGE(s.phase == Phase::MatchOver, "seed " << seed << " did not finish in " << cmd << " commands");
		if (s.phase == Phase::MatchOver) {
			matchesFinished++;
			CHECK(s.ranking.size() == 4);
		}
		totalTurns += s.turnNumber;
	}

	MESSAGE("soak: " << matchesFinished << "/60 matches finished, " << totalTurns << " turns, " << powersFired
					 << " powers fired");
	CHECK(matchesFinished == 60);
	CHECK(powersFired > 200);  // the soak must actually be exercising powers
}

TEST_CASE("soak: a frozen table still advances") {
	// Skip Turn could in principle starve the turn loop if every seat were frozen at once.
	MatchState s = test::allInYard();
	for (int p = 0; p < 4; p++) s.players[p].skipTurns = 3;
	TurnMachine tm(s, RulesConfig{});
	for (int i = 0; i < 40 && s.phase != Phase::MatchOver; i++) {
		int before = s.turnNumber;
		tm.roll(3);
		CHECK(s.turnNumber > before);  // the clock always moves
	}
}
