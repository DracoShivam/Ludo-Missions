// The eight effect kinds. Every one is checked for: what it will accept as a target, what the
// board looks like afterwards, and what it reports -- because the tray highlights exactly what
// targets() returns, so a disagreement between the two is a tap that visibly does nothing.

#include "doctest/doctest.h"

#include "Controllers/Logic/Powers/PowerCatalog.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "TestHelpers.h"

using namespace lm;

namespace {

// Compile one power from an inline definition, so each test states the params it depends on.
CompiledPowerPtr power(const std::string& id, const std::string& tier, const std::string& effect) {
	static PowerCatalog cat;
	auto r = cat.loadFromJson(R"({"powers":[{"id":")" + id + R"(","title":"t","tier":")" + tier + R"(","effect":)" + effect + "}]}");
	REQUIRE(r.errors.empty());
	REQUIRE(r.powers.size() == 1);
	return r.powers[0];
}

bool has(const std::vector<GameEvent>& evs, GameEventType t) {
	for (const auto& e : evs) {
		if (e.type == t) return true;
	}
	return false;
}

bool contains(const std::vector<PowerTarget>& v, PowerTarget t) {
	for (const auto& x : v) {
		if (x == t) return true;
	}
	return false;
}

// RED at 14 (global 14). GREEN at 4 (global 17). YELLOW on its own start cell (global 26, safe).
MatchState board() {
	return test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {0, -1, -1, -1}, {-1, -1, -1, -1}}});
}

}  // namespace

// -- diceDelta ---------------------------------------------------------------

TEST_CASE("diceDelta self: arms a modifier, and will not stack") {
	auto p = power("dice_plus", "common", R"({"type":"diceDelta","delta":3,"who":"self"})");
	auto s = board();
	CHECK(p->effect->targetKind() == TargetKind::None);
	REQUIRE(p->effect->targets({s, 0}).size() == 1);

	p->effect->apply(s, 0, {-1, -1});
	CHECK(s.players[0].diceDelta == 3);
	CHECK(p->effect->targets({s, 0}).empty());  // already armed: no second one
}

TEST_CASE("diceDelta opponent: targets living rivals only") {
	auto p = power("dice_minus", "rare", R"({"type":"diceDelta","delta":-3,"who":"opponent"})");
	auto s = board();
	CHECK(p->effect->targetKind() == TargetKind::OpponentPlayer);
	auto t = p->effect->targets({s, 0});
	CHECK(t.size() == 3);
	CHECK_FALSE(contains(t, {0, -1}));  // never yourself

	s.players[2].finishRank = 1;  // already out of the match
	CHECK(p->effect->targets({s, 0}).size() == 2);

	p->effect->apply(s, 0, {1, -1});
	CHECK(s.players[1].diceDelta == -3);
}

// -- diceForce / diceReroll --------------------------------------------------

TEST_CASE("diceForce: arms a fixed value, once") {
	auto p = power("force_six", "rare", R"({"type":"diceForce","value":6})");
	auto s = board();
	p->effect->apply(s, 0, {-1, -1});
	CHECK(s.players[0].forcedRoll == 6);
	CHECK(p->effect->targets({s, 0}).empty());
}

TEST_CASE("diceReroll: only offered once you have a roll to throw away") {
	auto p = power("reroll", "common", R"({"type":"diceReroll"})");
	auto s = board();
	s.phase = Phase::AwaitingRoll;
	CHECK(p->effect->targets({s, 0}).empty());  // nothing rolled yet

	s.phase = Phase::AwaitingMove;
	s.pendingRolls = {3};
	REQUIRE(p->effect->targets({s, 0}).size() == 1);

	p->effect->apply(s, 0, {-1, -1});
	CHECK(s.pendingRolls.empty());
	CHECK(s.phase == Phase::AwaitingRoll);
	CHECK(s.consecutiveSixes == 0);
}

// -- immunity ----------------------------------------------------------------

TEST_CASE("immunity: blocks capture and cannot be stacked") {
	auto p = power("protect", "rare", R"({"type":"immunity","turns":3})");
	auto s = board();
	p->effect->apply(s, 1, {-1, -1});
	CHECK(s.players[1].shieldTurns == 3);
	// RED at 14 rolling 3 would land on global 17 and take GREEN. Not any more.
	CHECK_FALSE(rules::capturableAt(s, 0, 17).has_value());
	CHECK(p->effect->targets({s, 1}).empty());
}

// -- sendHome (kick) ---------------------------------------------------------

TEST_CASE("sendHome: reaches rival tokens on the track, but not safe cells") {
	auto p = power("kick", "epic", R"({"type":"sendHome","respectSafeCells":true})");
	auto s = board();
	auto t = p->effect->targets({s, 0});
	REQUIRE(t.size() == 1);
	CHECK(t[0] == PowerTarget{1, 0});          // GREEN, out in the open
	CHECK_FALSE(contains(t, {2, 0}));          // YELLOW is on its start square
}

TEST_CASE("sendHome: can be configured to ignore safe cells") {
	auto p = power("kick_hard", "epic", R"({"type":"sendHome","respectSafeCells":false})");
	CHECK(p->effect->targets({board(), 0}).size() == 2);
}

TEST_CASE("sendHome: sends the token to the yard and reports TOKEN_KICKED, not a capture") {
	// A capture mission must not be completable by spending an item a mission handed you.
	auto p = power("kick", "epic", R"({"type":"sendHome"})");
	auto s = board();
	auto evs = p->effect->apply(s, 0, {1, 0});
	CHECK(s.players[1].progress[0] == IN_YARD);
	CHECK(has(evs, GameEventType::TOKEN_KICKED));
	CHECK(has(evs, GameEventType::POWER_USED));
	CHECK_FALSE(has(evs, GameEventType::TOKEN_CAPTURED));
}

TEST_CASE("sendHome: a shielded rival is out of reach") {
	auto p = power("kick", "epic", R"({"type":"sendHome"})");
	auto s = board();
	s.players[1].shieldTurns = 2;
	CHECK(p->effect->targets({s, 0}).empty());
}

TEST_CASE("sendHome: tokens in the yard or a home lane cannot be reached") {
	auto p = power("kick", "epic", R"({"type":"sendHome"})");
	auto s = test::makeState({{{14, -1, -1, -1}, {53, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(p->effect->targets({s, 0}).empty());
}

// -- swapTokens --------------------------------------------------------------

TEST_CASE("swapTokens: needs one of your own on the track") {
	auto p = power("swap", "epic", R"({"type":"swapTokens"})");
	auto none = test::makeState({{{-1, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(p->effect->targets({none, 0}).empty());
	CHECK_FALSE(p->effect->targets({board(), 0}).empty());
}

TEST_CASE("swapTokens: trades places with your least advanced token") {
	auto p = power("swap", "epic", R"({"type":"swapTokens"})");
	auto s = test::makeState({{{30, 8, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto evs = p->effect->apply(s, 0, {1, 0});
	CHECK(s.players[0].progress[1] == 4);   // ours at 8 was the least advanced
	CHECK(s.players[0].progress[0] == 30);  // the leader is untouched
	CHECK(s.players[1].progress[0] == 8);
	CHECK(has(evs, GameEventType::TOKEN_MOVED));
}

// -- skipTurn ----------------------------------------------------------------

TEST_CASE("skipTurn: arms a freeze on one rival, and will not stack") {
	auto p = power("freeze", "epic", R"({"type":"skipTurn","turns":1})");
	auto s = board();
	CHECK(p->effect->targets({s, 0}).size() == 3);
	p->effect->apply(s, 0, {2, -1});
	CHECK(s.players[2].skipTurns == 1);
	CHECK(p->effect->targets({s, 0}).size() == 2);
}

// -- leapTo ------------------------------------------------------------------

TEST_CASE("leapTo: moves a token forward and can capture on landing") {
	auto p = power("leap", "epic", R"({"type":"leapTo","progress":50})");
	auto s = board();
	auto t = p->effect->targets({s, 0});
	REQUIRE(t.size() == 1);
	CHECK(t[0] == PowerTarget{0, 0});

	auto evs = p->effect->apply(s, 0, {0, 0});
	CHECK(s.players[0].progress[0] == 50);
	CHECK(has(evs, GameEventType::TOKEN_MOVED));
}

TEST_CASE("leapTo: a token already past the landing point is not a target") {
	auto p = power("leap", "epic", R"({"type":"leapTo","progress":50})");
	auto s = test::makeState({{{52, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(p->effect->targets({s, 0}).empty());
}

TEST_CASE("leapTo: landing obeys the safe-cell rule, like any other landing") {
	// Put GREEN on global 21, a star. RED leaps onto it and must NOT capture.
	int greenOnStar = (21 - 13 + 52) % 52;
	auto p = power("leap_star", "epic", R"({"type":"leapTo","progress":21})");
	auto s = test::makeState({{{3, -1, -1, -1}, {greenOnStar, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto evs = p->effect->apply(s, 0, {0, 0});
	CHECK_FALSE(has(evs, GameEventType::TOKEN_CAPTURED));
	CHECK(s.players[1].progress[0] == greenOnStar);
}

// -- when a power may be spent ----------------------------------------------

TEST_CASE("diceDelta self: offered before the roll, not after") {
	// Using it once you have already thrown would defer the bonus to some later roll, which reads
	// as the power doing nothing at all.
	auto p = power("dice_plus", "common", R"({"type":"diceDelta","delta":3,"who":"self"})");
	auto s = board();
	s.phase = Phase::AwaitingRoll;
	CHECK(p->effect->targets({s, 0}).size() == 1);
	s.phase = Phase::AwaitingMove;
	CHECK(p->effect->targets({s, 0}).empty());
}

TEST_CASE("diceDelta on a rival is not tied to your own phase") {
	auto p = power("dice_minus", "rare", R"({"type":"diceDelta","delta":-3,"who":"opponent"})");
	auto s = board();
	s.phase = Phase::AwaitingMove;
	CHECK_FALSE(p->effect->targets({s, 0}).empty());
}

TEST_CASE("diceForce: offered before the roll only") {
	auto p = power("force_six", "rare", R"({"type":"diceForce","value":6})");
	auto s = board();
	s.phase = Phase::AwaitingRoll;
	CHECK(p->effect->targets({s, 0}).size() == 1);
	s.phase = Phase::AwaitingMove;
	CHECK(p->effect->targets({s, 0}).empty());
}

TEST_CASE("diceForce: will not hand you the third six") {
	// Two sixes already on the table; forcing a third forfeits the entire turn. A power that
	// actively hurts the player who spends it is worse than one that is briefly unavailable.
	auto p = power("force_six", "rare", R"({"type":"diceForce","value":6})");
	auto s = board();
	s.phase = Phase::AwaitingRoll;
	s.consecutiveSixes = 2;
	CHECK(p->effect->targets({s, 0}).empty());
	s.consecutiveSixes = 1;
	CHECK(p->effect->targets({s, 0}).size() == 1);
}

TEST_CASE("diceForce: a non-six force is unaffected by the six chain") {
	auto p = power("force_one", "rare", R"({"type":"diceForce","value":1})");
	auto s = board();
	s.phase = Phase::AwaitingRoll;
	s.consecutiveSixes = 2;
	CHECK(p->effect->targets({s, 0}).size() == 1);
}

TEST_CASE("player-targeted powers use the {player, -1} shape") {
	// The board has no token to point at for a whole-player power, so the controller expands these
	// into that player's tokens. The shape has to stay distinguishable or the expansion cannot work.
	auto jinx = power("jinx", "rare", R"({"type":"diceDelta","delta":-3,"who":"opponent"})");
	auto freeze = power("frz", "epic", R"({"type":"skipTurn","turns":1})");
	for (const auto& p : {jinx, freeze}) {
		CHECK(p->effect->targetKind() == TargetKind::OpponentPlayer);
		for (const auto& t : p->effect->targets({board(), 0})) {
			CHECK(t.token == -1);
			CHECK(t.player >= 0);
		}
	}
}

TEST_CASE("swapTokens: reports the rival's move as well as your own") {
	// Emitting only our half left the board drawing their token where it used to be.
	auto p = power("swap2", "epic", R"({"type":"swapTokens"})");
	auto s = test::makeState({{{30, 8, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto evs = p->effect->apply(s, 0, {1, 0});

	int moves = 0;
	bool ours = false, theirs = false;
	for (const auto& e : evs) {
		if (e.type != GameEventType::TOKEN_MOVED) continue;
		moves++;
		if (e.player == 0 && e.token == 1 && e.from == 8 && e.to == 4) ours = true;
		if (e.player == 1 && e.token == 0 && e.from == 4 && e.to == 8) theirs = true;
	}
	CHECK(moves == 2);
	CHECK(ours);
	CHECK(theirs);
}
