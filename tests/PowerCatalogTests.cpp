// The designer file and its parser. A broken row must name itself and be skipped, never take the
// rest of the catalogue down with it.

#include "doctest/doctest.h"

#include "Controllers/Logic/Powers/PowerCatalog.h"
#include "TestHelpers.h"

using namespace lm;

namespace {
PowerParseResult parse(const std::string& json) {
	PowerCatalog cat;
	return cat.loadFromJson(json);
}
}  // namespace

TEST_CASE("catalog: the shipped powers.json loads cleanly") {
	PowerCatalog cat;
	auto r = cat.loadFromJson(test::readContent("config/powers.json"));
	for (const auto& e : r.errors) MESSAGE(e);
	for (const auto& w : r.warnings) MESSAGE(w);
	CHECK(r.errors.empty());
	CHECK(r.warnings.empty());
	CHECK(r.powers.size() == 9);
}

TEST_CASE("catalog: every shipped power names a real effect and a real tier") {
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));
	for (const auto& p : cat.all()) {
		CHECK(p->effect != nullptr);
		CHECK_FALSE(p->def.title.empty());
		CHECK_FALSE(p->def.desc.empty());
		CHECK(p->def.weight > 0);
	}
}

TEST_CASE("catalog: the shipped set spans all three tiers") {
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));
	int perTier[3] = {0, 0, 0};
	for (const auto& p : cat.all()) perTier[(int) p->def.tier]++;
	MESSAGE("common " << perTier[0] << ", rare " << perTier[1] << ", epic " << perTier[2]);
	CHECK(perTier[0] > 0);
	CHECK(perTier[1] > 0);
	CHECK(perTier[2] > 0);
}

TEST_CASE("catalog: errors are per power and name the id") {
	auto r = parse(R"({"powers":[
		{"title":"no id","tier":"common","effect":{"type":"diceReroll"}},
		{"id":"bad_tier","title":"t","tier":"legendary","effect":{"type":"diceReroll"}},
		{"id":"bad_effect","title":"t","tier":"common","effect":{"type":"nope"}},
		{"id":"missing_param","title":"t","tier":"common","effect":{"type":"immunity"}},
		{"id":"dupe","title":"t","tier":"common","effect":{"type":"diceReroll"}},
		{"id":"dupe","title":"t","tier":"common","effect":{"type":"diceReroll"}},
		{"id":"good","title":"t","tier":"common","effect":{"type":"diceReroll"}}
	]})");
	CHECK(r.powers.size() == 2);  // "dupe" (first) and "good"
	REQUIRE(r.errors.size() == 5);
	CHECK(r.errors[0].find("missing \"id\"") != std::string::npos);
	CHECK(r.errors[1].find("bad_tier") != std::string::npos);
	CHECK(r.errors[2].find("unknown effect type 'nope'") != std::string::npos);
	CHECK(r.errors[3].find("missing required param 'turns'") != std::string::npos);
	CHECK(r.errors[4].find("duplicate") != std::string::npos);
}

TEST_CASE("catalog: an out-of-range effect param is rejected with a reason") {
	auto r = parse(R"({"powers":[{"id":"x","title":"t","tier":"rare","effect":{"type":"diceForce","value":9}}]})");
	CHECK(r.powers.empty());
	REQUIRE(r.errors.size() == 1);
	CHECK(r.errors[0].find("1..6") != std::string::npos);

	auto z = parse(R"({"powers":[{"id":"x","title":"t","tier":"common","effect":{"type":"diceDelta","delta":0}}]})");
	REQUIRE(z.errors.size() == 1);
	CHECK(z.errors[0].find("non-zero") != std::string::npos);
}

TEST_CASE("catalog: unknown keys warn but still load") {
	auto r = parse(R"({"powers":[{"id":"x","title":"t","tier":"common","tiar":"oops",
		"effect":{"type":"diceReroll","extra":1}}]})");
	CHECK(r.errors.empty());
	CHECK(r.powers.size() == 1);
	CHECK(r.warnings.size() == 2);
}

TEST_CASE("catalog: a disabled power parses but is not served") {
	auto r = parse(R"({"powers":[{"id":"x","title":"t","tier":"common","enabled":false,"effect":{"type":"diceReroll"}}]})");
	CHECK(r.errors.empty());
	CHECK(r.powers.empty());
}

TEST_CASE("catalog: draw returns a power of the requested tier") {
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));
	for (int tier = 0; tier <= 2; tier++) {
		Rng rng(7 + tier);
		for (int i = 0; i < 40; i++) {
			const CompiledPower* p = cat.draw((PowerTier) tier, rng);
			REQUIRE(p != nullptr);
			CHECK((int) p->def.tier == tier);
		}
	}
}

TEST_CASE("catalog: draw falls back down a tier rather than granting nothing") {
	PowerCatalog cat;
	cat.loadFromJson(R"({"powers":[{"id":"c","title":"t","tier":"common","effect":{"type":"diceReroll"}}]})");
	Rng rng(3);
	const CompiledPower* p = cat.draw(PowerTier::Epic, rng);
	REQUIRE(p != nullptr);
	CHECK(p->def.tier == PowerTier::Common);
}

TEST_CASE("catalog: an empty catalogue draws nothing rather than crashing") {
	PowerCatalog cat;
	cat.loadFromJson(R"({"powers":[]})");
	Rng rng(1);
	CHECK(cat.draw(PowerTier::Epic, rng) == nullptr);
	CHECK(cat.find("kick") == nullptr);
}

TEST_CASE("catalog: a wholly broken file leaves the previous catalogue standing") {
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));
	REQUIRE(cat.all().size() == 9);
	auto r = cat.loadFromJson("{ this is not json");
	CHECK_FALSE(r.errors.empty());
	CHECK(cat.all().size() == 9);  // hot reload of a typo must not disarm the feature mid-match
}

TEST_CASE("catalog: tier names round-trip") {
	for (auto t : {PowerTier::Common, PowerTier::Rare, PowerTier::Epic}) {
		PowerTier back;
		REQUIRE(powerTierFromString(powerTierName(t), back));
		CHECK(back == t);
	}
	PowerTier ignored;
	CHECK_FALSE(powerTierFromString("mythic", ignored));
}

// ---------------------------------------------------------------------------
// Every shipped power, end to end. The point is not to re-test each effect -- that is
// PowerEffectTests -- but to prove that no power in the catalogue is DEAD: that from a sensible
// board it offers a target, firing it changes something, and it reports an event. A power that
// mutates state and reports nothing is invisible to the board and reads as broken, which is
// exactly what happened to Send Home.
// ---------------------------------------------------------------------------

namespace {

// A board where something is possible for everyone: RED spread out, GREEN takeable in the open,
// YELLOW parked on a safe square, BLUE still in the yard.
MatchState liveBoard() {
	return test::makeState({{{14, 3, 30, -1}, {4, 20, -1, -1}, {0, -1, -1, -1}, {-1, -1, -1, -1}}});
}

// Powers that only make sense once you have rolled.
bool needsPendingRoll(const std::string& id) {
	return id == "reroll";
}

}  // namespace

TEST_CASE("every shipped power offers a target, fires, and reports something") {
	PowerCatalog cat;
	REQUIRE(cat.loadFromJson(test::readContent("config/powers.json")).errors.empty());
	REQUIRE(cat.all().size() == 9);

	for (const auto& p : cat.all()) {
		CAPTURE(p->def.id);
		MatchState s = liveBoard();
		if (needsPendingRoll(p->def.id)) {
			s.phase = Phase::AwaitingMove;
			s.pendingRolls = {3};
		}

		auto targets = p->effect->targets({s, 0});
		REQUIRE_MESSAGE(!targets.empty(), "no legal target for '" << p->def.id << "' on a live board");

		MatchState before = s;
		auto evs = p->effect->apply(s, 0, targets.front());
		REQUIRE_MESSAGE(!evs.empty(), "'" << p->def.id << "' reported no events, so the board can never redraw");

		// Something observable must have changed: token positions, a modifier, or the phase.
		bool changed = s.phase != before.phase || s.pendingRolls != before.pendingRolls;
		for (size_t i = 0; i < s.players.size() && !changed; i++) {
			const auto& a = before.players[i];
			const auto& b = s.players[i];
			changed = a.progress != b.progress || a.shieldTurns != b.shieldTurns || a.diceDelta != b.diceDelta ||
					  a.forcedRoll != b.forcedRoll || a.skipTurns != b.skipTurns;
		}
		CHECK_MESSAGE(changed, "'" << p->def.id << "' changed nothing at all");

		// And POWER_USED is always reported, which is what drives the on-screen confirmation.
		bool announced = false;
		for (const auto& e : evs) announced = announced || e.type == GameEventType::POWER_USED;
		CHECK_MESSAGE(announced, "'" << p->def.id << "' never emitted POWER_USED");
	}
}

TEST_CASE("board-moving powers emit an event the board actually renders") {
	// BoardView only redraws on TOKEN_MOVED, TOKEN_CAPTURED and TOKEN_KICKED. A power that shifts a
	// token without emitting one of those leaves the model and the board disagreeing.
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));

	for (const auto& p : cat.all()) {
		CAPTURE(p->def.id);
		MatchState s = liveBoard();
		if (needsPendingRoll(p->def.id)) {
			s.phase = Phase::AwaitingMove;
			s.pendingRolls = {3};
		}
		auto targets = p->effect->targets({s, 0});
		REQUIRE_FALSE(targets.empty());

		MatchState before = s;
		auto evs = p->effect->apply(s, 0, targets.front());

		bool tokensMoved = false;
		for (size_t i = 0; i < s.players.size(); i++) {
			tokensMoved = tokensMoved || before.players[i].progress != s.players[i].progress;
		}
		if (!tokensMoved) {
			continue;  // a state-only power; its feedback is the PowerUsedMsg line
		}
		bool renderable = false;
		for (const auto& e : evs) {
			renderable = renderable || e.type == GameEventType::TOKEN_MOVED || e.type == GameEventType::TOKEN_CAPTURED ||
						 e.type == GameEventType::TOKEN_KICKED;
		}
		CHECK_MESSAGE(renderable, "'" << p->def.id << "' moved tokens without an event the board can draw");
	}
}

TEST_CASE("a power that moves a rival's token says so, not just its own") {
	// Swap shifts two tokens. Reporting only one leaves the other drawn in its old place.
	PowerCatalog cat;
	cat.loadFromJson(test::readContent("config/powers.json"));
	const CompiledPower* swap = cat.find("swap");
	REQUIRE(swap != nullptr);

	MatchState s = liveBoard();
	auto targets = swap->effect->targets({s, 0});
	REQUIRE_FALSE(targets.empty());
	MatchState before = s;
	auto evs = swap->effect->apply(s, 0, targets.front());

	for (size_t seat = 0; seat < s.players.size(); seat++) {
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			if (before.players[seat].progress[t] == s.players[seat].progress[t]) continue;
			bool reported = false;
			for (const auto& e : evs) {
				reported = reported || ((e.type == GameEventType::TOKEN_MOVED || e.type == GameEventType::TOKEN_KICKED ||
										 e.type == GameEventType::TOKEN_CAPTURED) &&
										((e.player == (int) seat && e.token == t) ||
										 (e.victimPlayer == (int) seat && e.victimToken == t)));
			}
			CHECK_MESSAGE(reported, "seat " << seat << " token " << t << " moved with no event naming it");
		}
	}
}
