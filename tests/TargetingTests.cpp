// The targeting state machine. This file exists because the previous implementation kept this
// state as a raw member inside GameController, where it could not be tested -- and it shipped a
// bug that made the game permanently unplayable after one mis-tap.

#include "doctest/doctest.h"

#include "Controllers/Logic/Powers/TargetingSession.h"

using namespace lm;
using TR = TargetingSession::TapResult;

namespace {
const std::vector<PowerTarget> TWO_RIVALS = {{1, 0}, {2, 3}};
}

TEST_CASE("targeting: a fresh session is inactive and ignores taps") {
	TargetingSession s;
	CHECK_FALSE(s.active());
	CHECK(s.tap({1, 0}) == TR::Ignored);
}

TEST_CASE("targeting: begin arms the session with its legal targets") {
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
	CHECK(s.active());
	CHECK(s.powerId() == "kick");
	CHECK(s.targetKind() == TargetKind::OpponentToken);
	CHECK(s.targets().size() == 2);
	CHECK(s.accepts({1, 0}));
	CHECK_FALSE(s.accepts({3, 0}));
}

TEST_CASE("targeting: arming with no legal targets refuses rather than trapping the player") {
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, {});
	CHECK_FALSE(s.active());
}

TEST_CASE("targeting: a legal tap applies and ends the session") {
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
	CHECK(s.tap({2, 3}) == TR::Applied);
	CHECK(s.chosen() == PowerTarget{2, 3});
	CHECK_FALSE(s.active());
}

TEST_CASE("targeting: an ILLEGAL tap cancels -- it is never swallowed") {
	// The exact regression. Previously an illegal tap left the session armed, and from then on
	// every tap on every token was routed into the power and discarded.
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
	CHECK(s.tap({0, 0}) == TR::Cancelled);  // the player's own token
	CHECK_FALSE(s.active());
}

TEST_CASE("targeting: after an illegal tap the next tap is the caller's again") {
	// The bug in one test: arm a power, mis-tap, then try to move your own token.
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
	REQUIRE(s.tap({0, 0}) == TR::Cancelled);
	CHECK(s.tap({0, 1}) == TR::Ignored);  // Ignored = "handle this as a normal move"
}

TEST_CASE("targeting: cancel is idempotent and always available") {
	TargetingSession s;
	s.cancel();
	CHECK_FALSE(s.active());
	s.begin("protect", TargetKind::None, {{-1, -1}});
	s.cancel();
	s.cancel();
	CHECK_FALSE(s.active());
	CHECK(s.tap({-1, -1}) == TR::Ignored);
}

TEST_CASE("targeting: re-arming replaces the previous session cleanly") {
	TargetingSession s;
	s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
	s.begin("freeze", TargetKind::OpponentPlayer, {{1, -1}});
	CHECK(s.powerId() == "freeze");
	CHECK(s.targets().size() == 1);
	CHECK_FALSE(s.accepts({2, 3}));  // the old target set is gone
	CHECK(s.tap({1, -1}) == TR::Applied);
}

TEST_CASE("targeting: a self-targeted power uses the no-target sentinel") {
	TargetingSession s;
	s.begin("protect", TargetKind::None, {{-1, -1}});
	CHECK(s.active());
	CHECK(s.tap({-1, -1}) == TR::Applied);
	CHECK(s.chosen() == PowerTarget{-1, -1});
}

TEST_CASE("targeting: every tap resolves the session, whatever it is") {
	// The invariant that makes the whole class of bug impossible: no tap can leave it armed.
	const std::vector<PowerTarget> taps = {{0, 0}, {1, 0}, {2, 3}, {3, 2}, {-1, -1}, {9, 9}};
	for (const auto& t : taps) {
		TargetingSession s;
		s.begin("kick", TargetKind::OpponentToken, TWO_RIVALS);
		TR r = s.tap(t);
		CHECK((r == TR::Applied || r == TR::Cancelled));
		CHECK_FALSE(s.active());
	}
}
