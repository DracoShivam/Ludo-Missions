#include "doctest/doctest.h"

#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "TestHelpers.h"

using namespace lm;
using ET = GameEventType;

TEST_CASE("turn: start match") {
	auto s = test::allInYard();
	TurnMachine m(s, RulesConfig{});
	auto ev = m.startMatch();
	CHECK(test::types(ev) == std::vector<ET>{ET::MATCH_STARTED, ET::TURN_STARTED});
	CHECK(s.phase == Phase::AwaitingRoll);
	CHECK(s.turnNumber == 1);
}

TEST_CASE("turn: no moves passes the turn") {
	auto s = test::allInYard();
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	auto ev = m.roll(3);
	CHECK(test::types(ev) == std::vector<ET>{ET::DICE_ROLLED, ET::NO_MOVES, ET::TURN_ENDED, ET::TURN_STARTED});
	CHECK(s.current == 1);
	CHECK(ev.back().player == 1);
	CHECK(ev[2].player == 0);
}

TEST_CASE("turn: stacked six then move both") {
	auto s = test::allInYard();
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	auto e1 = m.roll(6);
	CHECK(test::types(e1) == std::vector<ET>{ET::DICE_ROLLED});
	CHECK(s.phase == Phase::AwaitingRoll);
	m.roll(4);
	CHECK(s.phase == Phase::AwaitingMove);
	CHECK(s.pendingRolls == std::vector<int>{6, 4});
	// 4 alone is illegal for yard tokens
	CHECK(m.move(0, 4).empty());
	auto e2 = m.move(0, 6);
	CHECK(test::contains(e2, ET::TOKEN_UNLOCKED));
	CHECK(s.phase == Phase::AwaitingMove);
	auto e3 = m.move(0, 4);
	CHECK(test::types(e3) == std::vector<ET>{ET::TOKEN_MOVED, ET::TURN_ENDED, ET::TURN_STARTED});
	CHECK(s.players[0].progress[0] == 4);
	CHECK(e3[0].steps == 4);
	CHECK(e2[0].steps == 1);
}

TEST_CASE("turn: three sixes forfeit") {
	auto s = test::makeState({{{10, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	m.roll(6);
	m.roll(6);
	auto ev = m.roll(6);
	CHECK(test::types(ev) == std::vector<ET>{ET::DICE_ROLLED, ET::THREE_SIXES, ET::TURN_ENDED, ET::TURN_STARTED});
	CHECK(s.current == 1);
	CHECK(s.players[0].progress[0] == 10);
}

TEST_CASE("turn: three sixes keep older pending rolls from before a bonus") {
	// RED at 12 captures GREEN at global 17 with a 5 -> bonus roll with pending {6} kept... build it directly:
	auto s = test::makeState({{{12, 30, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	m.roll(6);
	m.roll(5);
	auto cap = m.move(0, 5);  // 12 -> 17 captures GREEN
	CHECK(test::contains(cap, ET::TOKEN_CAPTURED));
	CHECK(test::contains(cap, ET::BONUS_ROLL_GRANTED));
	CHECK(s.phase == Phase::AwaitingRoll);
	CHECK(s.pendingRolls == std::vector<int>{6});
	// consecutiveSixes was reset by the 5, so 6,6,6 now forfeits only those three
	m.roll(6);
	m.roll(6);
	auto ev = m.roll(6);
	CHECK(test::contains(ev, ET::THREE_SIXES));
	CHECK(s.phase == Phase::AwaitingMove);
	CHECK(s.pendingRolls == std::vector<int>{6});
}

TEST_CASE("turn: capture sends victim home and grants bonus, pending kept") {
	auto s = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	m.roll(3);
	auto ev = m.move(0, 3);
	REQUIRE(test::contains(ev, ET::TOKEN_CAPTURED));
	CHECK(s.players[1].progress[0] == -1);
	CHECK(s.phase == Phase::AwaitingRoll);
	CHECK(s.current == 0);
	for (auto& e : ev) {
		if (e.type == ET::TOKEN_CAPTURED) {
			CHECK(e.victimPlayer == 1);
			CHECK(e.cell == 17);
		}
		if (e.type == ET::BONUS_ROLL_GRANTED) CHECK(e.reason == BONUS_CAPTURE);
	}
}

TEST_CASE("turn: home lane and finish events") {
	auto s = test::makeState({{{48, 54, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	m.roll(4);
	auto ev = m.move(0, 4);  // 48 -> 52
	CHECK(test::contains(ev, ET::TOKEN_ENTERED_HOME_LANE));
	s.current = 0;
	s.phase = Phase::AwaitingRoll;
	m.roll(2);
	auto fin = m.move(1, 2);  // 54 -> 56
	CHECK(test::contains(fin, ET::TOKEN_FINISHED));
	CHECK(test::contains(fin, ET::BONUS_ROLL_GRANTED));
	CHECK_FALSE(test::contains(fin, ET::TOKEN_ENTERED_HOME_LANE));
}

TEST_CASE("turn: finishing the 4th token ends the match") {
	auto s = test::makeState({{{56, 56, 56, 53}, {10, -1, -1, -1}, {20, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	m.roll(3);
	auto ev = m.move(3, 3);
	CHECK(test::types(ev) ==
		  std::vector<ET>{ET::TOKEN_MOVED, ET::TOKEN_FINISHED, ET::PLAYER_FINISHED, ET::TURN_ENDED, ET::MATCH_ENDED});
	CHECK(s.phase == Phase::MatchOver);
	CHECK(s.ranking == std::vector<int>{0, 2, 1, 3});
	CHECK(m.roll(3).empty());
}

TEST_CASE("turn: sitsOut players are skipped") {
	auto s = test::allInYard();
	for (int p = 1; p < 4; p++) s.players[p].sitsOut = true;
	TurnMachine m(s, RulesConfig{});
	m.startMatch();
	auto ev = m.roll(2);
	CHECK(s.current == 0);
	CHECK(ev.back().type == ET::TURN_STARTED);
	CHECK(ev.back().player == 0);
}

TEST_CASE("sim: 200 seeded all-bot matches terminate") {
	for (uint32_t seed = 1; seed <= 200; seed++) {
		auto s = test::allInYard();
		Rng rng(seed);
		TurnMachine m(s, RulesConfig{});
		m.startMatch();
		int commands = 0;
		while (s.phase != Phase::MatchOver && commands < 5000) {
			if (s.phase == Phase::AwaitingRoll) {
				m.roll(rng.dice());
			} else {
				auto opts = rules::legalMoves(s, s.current);
				REQUIRE_FALSE(opts.empty());
				auto o = BotBrain::choose(s, opts, rng);
				REQUIRE_FALSE(m.move(o.token, o.value).empty());
			}
			commands++;
		}
		CHECK_MESSAGE(s.phase == Phase::MatchOver, "seed " << seed);
		CHECK(s.ranking.size() == 4);
	}
}
