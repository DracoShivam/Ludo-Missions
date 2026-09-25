#include "doctest/doctest.h"

#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Rules.h"
#include "TestHelpers.h"

using namespace lm;

TEST_CASE("bot: prefers capture over unlock") {
	auto s = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.pendingRolls = {3, 6};
	Rng rng(1);
	// 6 would unlock; 3 captures
	auto o = BotBrain::choose(s, rules::legalMoves(s, 0), rng);
	CHECK(o.captures);
	CHECK(o.value == 3);
}

TEST_CASE("bot: escapes a threat") {
	// RED token0 at 20 (global 20) threatened by GREEN at global 17 (3 behind). token1 at 2.
	auto s = test::makeState({{{20, 2, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.pendingRolls = {1};  // token0 -> 21 (global 21 is a safe star)
	Rng rng(1);
	auto o = BotBrain::choose(s, rules::legalMoves(s, 0), rng);
	CHECK(o.token == 0);
}

TEST_CASE("bot: deterministic with seed") {
	auto s = test::makeState({{{5, 10, 15, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.pendingRolls = {2};
	Rng a(42), b(42);
	auto opts = rules::legalMoves(s, 0);
	CHECK(BotBrain::choose(s, opts, a).token == BotBrain::choose(s, opts, b).token);
}
