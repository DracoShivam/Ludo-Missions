// Powers where they touch the rules: the die, the turn order, and capture. These run through the
// real TurnMachine, because a modifier that works in isolation and not in the turn loop is no use.

#include "doctest/doctest.h"

#include <algorithm>

#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "TestHelpers.h"

using namespace lm;
using ET = GameEventType;

namespace {

MatchState oneOut() {
	return test::makeState({{{10, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
}

bool has(const std::vector<GameEvent>& evs, ET t) {
	for (const auto& e : evs) {
		if (e.type == t) return true;
	}
	return false;
}

}  // namespace

// -- dice modifiers ----------------------------------------------------------

TEST_CASE("dice: a delta changes the value that is spent, and is one-shot") {
	auto s = oneOut();
	s.players[0].diceDelta = 3;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(2);
	REQUIRE(s.pendingRolls.size() == 1);
	CHECK(s.pendingRolls[0] == 5);
	CHECK(s.players[0].diceDelta == 0);  // spent
}

TEST_CASE("dice: a boost OVERFLOWS past a die face") {
	// Rolling a 6 with +3 is worth 9. Clamping to 6 made the power worth nothing on exactly the
	// roll a player is most pleased to see.
	auto s = oneOut();
	s.players[0].diceDelta = 3;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(6);
	CHECK(s.pendingRolls[0] == 9);
}

TEST_CASE("dice: a penalty never drops below a movable roll") {
	// -3 on a 1 must not produce 0 or a negative: a roll that cannot be spent would strand the turn.
	for (int die = 1; die <= 6; die++) {
		auto s = oneOut();
		s.players[0].diceDelta = -3;
		TurnMachine tm(s, RulesConfig{});
		tm.roll(die);
		REQUIRE(s.pendingRolls.size() == 1);
		CHECK(s.pendingRolls[0] >= 1);
		CHECK(s.pendingRolls[0] == std::max(1, die - 3));
	}
}

TEST_CASE("dice: a boosted roll is spendable by the rules") {
	// targetProgress used to reject anything above 6, which would have made a 9 unusable.
	CHECK(rules::targetProgress(10, 9) == 19);
	CHECK(rules::targetProgress(IN_YARD, 9) == 0);            // 6 or better unlocks
	CHECK(rules::targetProgress(IN_YARD, 5) == INVALID_PROGRESS);
	CHECK(rules::targetProgress(50, 9) == INVALID_PROGRESS);  // still cannot overshoot the centre
}

TEST_CASE("dice: the six rules read the DIE, not the boosted total") {
	// A natural 6 boosted to 9 keeps the extra roll it earned...
	{
		auto s = oneOut();
		s.players[0].diceDelta = 3;
		TurnMachine tm(s, RulesConfig{});
		tm.roll(6);
		CHECK(s.pendingRolls[0] == 9);
		CHECK(s.phase == Phase::AwaitingRoll);
		CHECK(s.consecutiveSixes == 1);
	}
	// ...and a 3 boosted to 6 does NOT get one it never earned.
	{
		auto s = oneOut();
		s.players[0].diceDelta = 3;
		TurnMachine tm(s, RulesConfig{});
		tm.roll(3);
		CHECK(s.pendingRolls[0] == 6);
		CHECK(s.phase == Phase::AwaitingMove);
		CHECK(s.consecutiveSixes == 0);
	}
}

TEST_CASE("dice: a forced roll overrides the die and is one-shot") {
	auto s = oneOut();
	s.players[0].forcedRoll = 6;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(1);
	CHECK(s.pendingRolls[0] == 6);
	CHECK(s.players[0].forcedRoll == 0);
	CHECK(s.phase == Phase::AwaitingRoll);
}

TEST_CASE("dice: force applies before delta") {
	auto s = oneOut();
	s.players[0].forcedRoll = 6;
	s.players[0].diceDelta = -3;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(1);
	CHECK(s.pendingRolls[0] == 3);
}

TEST_CASE("dice: a modifier belongs to one player only") {
	auto s = test::makeState({{{10, -1, -1, -1}, {10, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.players[1].diceDelta = 3;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(2);  // player 0 rolls; player 1's modifier must not apply
	CHECK(s.pendingRolls[0] == 2);
	CHECK(s.players[1].diceDelta == 3);
}

// -- freeze ------------------------------------------------------------------

TEST_CASE("freeze: a frozen seat is skipped and loses one charge") {
	auto s = test::allInYard();
	s.players[1].skipTurns = 1;
	TurnMachine tm(s, RulesConfig{});
	auto evs = tm.roll(3);  // nothing to move, so the turn ends and the next seat is chosen
	CHECK(has(evs, ET::TURN_SKIPPED));
	CHECK(s.current == 2);  // 1 was skipped
	CHECK(s.players[1].skipTurns == 0);
}

TEST_CASE("freeze: two charges skip two of that player's turns") {
	auto s = test::allInYard();
	s.players[1].skipTurns = 2;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(3);
	REQUIRE(s.current == 2);
	CHECK(s.players[1].skipTurns == 1);
	tm.roll(3);  // player 2
	CHECK(s.current == 3);
	tm.roll(3);  // player 3 -> wraps to 0, 1 still frozen
	CHECK(s.current == 0);
	CHECK(s.players[1].skipTurns == 1);
}

TEST_CASE("freeze: with every rival frozen the current player simply goes again") {
	auto s = test::allInYard();
	for (int p = 1; p < 4; p++) s.players[p].skipTurns = 1;
	TurnMachine tm(s, RulesConfig{});
	tm.roll(3);
	CHECK(s.current == 0);
	for (int p = 1; p < 4; p++) CHECK(s.players[p].skipTurns == 0);
}

// -- immunity ----------------------------------------------------------------

TEST_CASE("immunity: ticks on its owner's turns only, and announces its end") {
	auto s = oneOut();
	s.players[0].shieldTurns = 1;
	TurnMachine tm(s, RulesConfig{});

	// An opponent's turn must not burn it, so start by handing the turn over.
	s.current = 1;
	tm.roll(3);  // player 1 has nothing to move; their turn ends
	CHECK(s.players[0].shieldTurns == 1);

	while (s.current != 0 && s.phase != Phase::MatchOver) tm.roll(3);
	auto evs = tm.roll(3);
	if (s.phase == Phase::AwaitingMove) {
		auto opts = rules::legalMoves(s, 0);
		REQUIRE_FALSE(opts.empty());
		evs = tm.move(opts[0].token, opts[0].value);
	}
	CHECK(s.players[0].shieldTurns == 0);
	CHECK(has(evs, ET::SHIELD_EXPIRED));
}

TEST_CASE("immunity: a shielded token is not a victim and does not block the cell") {
	// GREEN shielded on global 17; RED landing there captures nothing and is not itself blocked.
	auto s = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.players[1].shieldTurns = 2;
	CHECK_FALSE(rules::capturableAt(s, 0, 17).has_value());
	auto moves = rules::legalMoves(s, 0);
	bool canLand = false;
	for (const auto& m : moves) canLand = canLand || m.to == 17;
	s.pendingRolls = {3};
	moves = rules::legalMoves(s, 0);
	for (const auto& m : moves) canLand = canLand || m.to == 17;
	CHECK(canLand);
}

// -- the rules powers must not have broken ----------------------------------

TEST_CASE("dice: with no modifiers set, rolling is exactly as it was") {
	auto s = oneOut();
	TurnMachine tm(s, RulesConfig{});
	tm.roll(4);
	CHECK(s.pendingRolls[0] == 4);
	CHECK(s.phase == Phase::AwaitingMove);
	auto s2 = test::allInYard();
	TurnMachine tm2(s2, RulesConfig{});
	tm2.roll(6);
	CHECK(s2.pendingRolls[0] == 6);
	CHECK(s2.phase == Phase::AwaitingRoll);
	CHECK(s2.consecutiveSixes == 1);
}

// -- the signal a modifier actually fired ------------------------------------

TEST_CASE("dice: DICE_ROLLED reports the raw die alongside the modified value") {
	// Without this the player rolls, sees a 5, and cannot tell whether it was a natural 5 or a 2
	// that their power boosted -- which is indistinguishable from the power not working.
	auto s = oneOut();
	s.players[0].diceDelta = 3;
	TurnMachine tm(s, RulesConfig{});
	auto evs = tm.roll(2);
	REQUIRE_FALSE(evs.empty());
	CHECK(evs[0].type == ET::DICE_ROLLED);
	CHECK(evs[0].from == 2);   // what the die actually showed
	CHECK(evs[0].value == 5);  // what the player spends
}

TEST_CASE("dice: an unmodified roll reports the same number twice") {
	auto s = oneOut();
	TurnMachine tm(s, RulesConfig{});
	auto evs = tm.roll(4);
	CHECK(evs[0].from == 4);
	CHECK(evs[0].value == 4);
}

TEST_CASE("dice: a forced roll is reported as a modification too") {
	auto s = oneOut();
	s.players[0].forcedRoll = 6;
	TurnMachine tm(s, RulesConfig{});
	auto evs = tm.roll(1);
	CHECK(evs[0].from == 1);
	CHECK(evs[0].value == 6);
}
