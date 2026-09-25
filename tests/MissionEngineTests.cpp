#include "doctest/doctest.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "Controllers/Logic/Missions/MissionEngine.h"
#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Rng.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "TestHelpers.h"

using namespace lm;
using K = MissionUpdate::Kind;
using ET = GameEventType;

namespace {

// Extract one mission object (by id) from the real starter file, so tests track designer data.
std::string onlyMission(const std::string& id) {
	std::string all = test::readContent("config/missions.json");
	rapidjson::Document d;
	d.Parse(all.c_str());
	rapidjson::Document out;
	out.SetObject();
	rapidjson::Value arr(rapidjson::kArrayType);
	for (auto& m : d["missions"].GetArray()) {
		if (std::string(m["id"].GetString()) == id) arr.PushBack(rapidjson::Value(m, out.GetAllocator()), out.GetAllocator());
	}
	out.AddMember("missions", arr, out.GetAllocator());
	rapidjson::StringBuffer sb;
	rapidjson::Writer<rapidjson::StringBuffer> w(sb);
	out.Accept(w);
	return sb.GetString();
}

struct Harness {
	MissionEngine engine;
	MatchState state = test::allInYard();
	explicit Harness(const std::string& json, MissionSettings s = {}) {
		engine.setSettings(s);
		auto r = engine.loadFromJson(json);
		REQUIRE(r.errors.empty());
		engine.startMatch(0, 7);
	}
	std::vector<MissionUpdate> feed(const GameEvent& e) { return engine.onEvent(e, state); }
	std::vector<MissionUpdate> turnStart() { return feed(test::ev(ET::TURN_STARTED, 0)); }
	std::vector<MissionUpdate> turnEnd() { return feed(test::ev(ET::TURN_ENDED, 0)); }
};

bool has(const std::vector<MissionUpdate>& u, K kind, const std::string& id = "") {
	for (auto& x : u)
		if (x.kind == kind && (id.empty() || x.instance.id == id)) return true;
	return false;
}

std::string alwaysMission(const std::string& id, int turns = 3, int cooldown = 2) {
	return R"({"id":")" + id + R"(","title":"t","reward":{"coins":1},"turns":)" + std::to_string(turns) + R"(,"cooldownTurns":)" +
		   std::to_string(cooldown) + R"(,"objective":{"type":"count","event":"TOKEN_FINISHED","where":{"player":"self"},"target":1}})";
}

}  // namespace

TEST_CASE("engine: roll_six_now completes on a 6, fails at turn end otherwise") {
	Harness h(onlyMission("roll_six_now"));
	auto u = h.turnStart();
	REQUIRE(has(u, K::Offered, "roll_six_now"));
	CHECK(u[0].instance.turnsLeft == 1);
	CHECK(u[0].instance.description == "Roll a 6 this turn");
	CHECK(has(h.feed(test::rolled(0, 6)), K::Completed));

	Harness h2(onlyMission("roll_six_now"));
	h2.turnStart();
	CHECK(h2.feed(test::rolled(0, 3)).empty());
	CHECK(h2.feed(test::rolled(1, 6)).empty());  // enemy 6 doesn't count
	CHECK(has(h2.turnEnd(), K::Failed));
}

TEST_CASE("engine: six_streak_2") {
	Harness h(onlyMission("six_streak_2"));
	h.turnStart();
	h.feed(test::rolled(0, 6));
	auto u = h.turnEnd();
	CHECK_FALSE(has(u, K::Failed));  // (2-1) > 1 is false
	h.turnStart();
	h.feed(test::rolled(0, 6));
	CHECK(has(h.turnEnd(), K::Completed));

	Harness f(onlyMission("six_streak_2"));
	f.turnStart();
	f.feed(test::rolled(0, 2));
	CHECK(has(f.turnEnd(), K::Failed));  // fail-fast: (2-0) > 1
}

TEST_CASE("engine: survive_4 (avoid)") {
	Harness h(onlyMission("survive_4"));
	h.state = test::makeState({{{20, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});  // GREEN 3 behind
	REQUIRE(has(h.turnStart(), K::Offered));
	CHECK(has(h.feed(test::captured(1, 0)), K::Failed));

	Harness ok(onlyMission("survive_4"));
	ok.state = h.state;
	ok.turnStart();
	for (int i = 0; i < 3; i++) {
		CHECK_FALSE(has(ok.turnEnd(), K::Completed));
		ok.feed(test::captured(1, 2));  // someone else captured: fine
		ok.turnStart();
	}
	auto u = ok.turnEnd();
	REQUIRE(has(u, K::Completed));
	CHECK(u[0].instance.progress == 4);
}

TEST_CASE("engine: three_out_4 (state) not offered if already satisfied, completes on move") {
	Harness h(onlyMission("three_out_4"));
	h.state = test::makeState({{{1, 2, 3, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(h.turnStart().empty());

	Harness g(onlyMission("three_out_4"));
	g.state = test::makeState({{{1, 2, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	REQUIRE(has(g.turnStart(), K::Offered));
	g.state.players[0].progress[2] = 0;
	CHECK(has(g.feed(test::ev(ET::TOKEN_MOVED, 0)), K::Completed));
}

TEST_CASE("engine: move_25_in_3 sums steps") {
	Harness h(onlyMission("move_25_in_3"));
	h.state = test::makeState({{{5, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	REQUIRE(has(h.turnStart(), K::Offered));
	GameEvent m = test::ev(ET::TOKEN_MOVED, 0);
	m.steps = 12;
	auto u = h.feed(m);
	REQUIRE(has(u, K::Progress));
	CHECK(u[0].instance.progress == 12);
	CHECK(u[0].instance.description == "Move 25 cells");  // progress and turns live in the card footer
	m.steps = 13;
	CHECK(has(h.feed(m), K::Completed));
}

TEST_CASE("engine: caps, no duplicates, cooldown, voiding") {
	std::string json = "{\"missions\":[" + alwaysMission("a") + "," + alwaysMission("b") + "," + alwaysMission("c") + "," + alwaysMission("d") + "]}";
	MissionSettings s;
	s.offersPerTurnStart = 3;
	s.offersPerTurn = 3;
	Harness h(json, s);
	auto u = h.turnStart();
	CHECK(u.size() == 3);  // maxActive 3
	CHECK(h.engine.activeInstances().size() == 3);
	h.turnEnd();
	CHECK(h.turnStart().empty());  // full
	auto v = h.feed(test::ev(ET::MATCH_ENDED, 0));
	CHECK(v.size() == 3);
	CHECK(has(v, K::Voided));
	CHECK(h.engine.activeInstances().empty());

	// offersPerTurnStart = 1 default
	Harness one(json);
	CHECK(one.turnStart().size() == 1);
}

TEST_CASE("engine: cooldown blocks re-offer until turn j + cooldown + 1") {
	// Cooldown is a pacing preference, so it only holds when missions are rationed. With alwaysOn the player is
	// never left idle and the cooldown gives way -- that case is covered by the always-on tests below.
	MissionSettings rationed;
	rationed.alwaysOn = false;
	Harness h("{\"missions\":[" + alwaysMission("a", 1, 2) + "]}", rationed);
	REQUIRE(has(h.turnStart(), K::Offered));  // turn 1
	REQUIRE(has(h.turnEnd(), K::Failed));     // resolved in turn 1 -> eligible at turn 4
	CHECK(h.turnStart().empty());             // 2
	h.turnEnd();
	CHECK(h.turnStart().empty());  // 3
	h.turnEnd();
	CHECK(has(h.turnStart(), K::Offered));  // 4
}

// ---------------------------------------------------------------------------
// Always-on: the player should never start a turn without a live mission.
// ---------------------------------------------------------------------------

TEST_CASE("always-on: the sole mission is re-offered immediately despite its cooldown") {
	Harness h("{\"missions\":[" + alwaysMission("a", 1, 5) + "]}");  // cooldown 5, window 1
	REQUIRE(has(h.turnStart(), K::Offered));
	REQUIRE(has(h.turnEnd(), K::Failed));
	// Rationed, this would stay quiet for five turns.
	CHECK(has(h.turnStart(), K::Offered));
	REQUIRE(has(h.turnEnd(), K::Failed));
	CHECK(has(h.turnStart(), K::Offered));
}

TEST_CASE("always-on: every turn over a long run starts with a mission live") {
	Harness h("{\"missions\":[" + alwaysMission("a", 1, 3) + "]}");
	for (int turn = 0; turn < 25; turn++) {
		h.turnStart();
		CHECK_MESSAGE(!h.engine.activeInstances().empty(), "no live mission at turn " << turn);
		h.turnEnd();
	}
}

TEST_CASE("always-on: maxActive still holds once a mission is live") {
	// The caps exist to stop mission spam, not to enforce idle time. Waiving them when the HUD is empty must not
	// turn into waiving them always: with one long mission live and maxActive 1, nothing more may be offered.
	MissionSettings s;
	s.maxActive = 1;
	Harness h("{\"missions\":[" + alwaysMission("a", 9, 0) + "," + alwaysMission("b", 9, 0) + "]}", s);
	REQUIRE(h.turnStart().size() == 1);
	CHECK(h.engine.activeInstances().size() == 1);
	h.turnEnd();
	CHECK(h.turnStart().empty());  // still live, still capped
	CHECK(h.engine.activeInstances().size() == 1);
}

TEST_CASE("always-on: one offer per moment is still the rule when nothing is live") {
	// Waiving the caps buys exactly one mission, not a burst of three.
	Harness h("{\"missions\":[" + alwaysMission("a", 9, 0) + "," + alwaysMission("b", 9, 0) + "," + alwaysMission("c", 9, 0) + "]}");
	CHECK(h.turnStart().size() == 1);
	CHECK(h.engine.activeInstances().size() == 1);
}

TEST_CASE("always-on can be switched off") {
	MissionSettings s;
	s.alwaysOn = false;
	Harness h("{\"missions\":[" + alwaysMission("a", 1, 5) + "]}", s);
	REQUIRE(has(h.turnStart(), K::Offered));
	REQUIRE(has(h.turnEnd(), K::Failed));
	CHECK(h.turnStart().empty());
}

TEST_CASE("engine: hot reload keeps active instances alive") {
	Harness h(onlyMission("roll_six_now"));
	REQUIRE(has(h.turnStart(), K::Offered));
	h.engine.loadFromJson(R"({"missions":[]})");
	CHECK(has(h.feed(test::rolled(0, 6)), K::Completed));
}

TEST_CASE("engine: strike_now only at afterRoll with a capturing roll") {
	Harness h(onlyMission("strike_now"));
	h.state = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(h.turnStart().empty());
	h.state.pendingRolls = {3};
	h.state.phase = Phase::AwaitingMove;
	auto u = h.feed(test::rolled(0, 3));
	REQUIRE(has(u, K::Offered, "strike_now"));

	Harness n(onlyMission("strike_now"));
	n.state = h.state;
	n.state.pendingRolls = {2};  // no capture with a 2
	n.turnStart();
	CHECK(n.feed(test::rolled(0, 2)).empty());
}

TEST_CASE("integration: Hunter scenario through the real TurnMachine") {
	Harness h(onlyMission("capture_in_3"));
	h.state = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	TurnMachine m(h.state, RulesConfig{});
	std::vector<MissionUpdate> all;
	auto run = [&](const std::vector<GameEvent>& evs) {
		for (auto& e : evs)
			for (auto& u : h.feed(e)) all.push_back(u);
	};
	run(m.startMatch());
	REQUIRE(has(all, K::Offered, "capture_in_3"));
	run(m.roll(3));
	run(m.move(0, 3));
	CHECK(has(all, K::Completed, "capture_in_3"));
}

// ---------------------------------------------------------------------------
// The always-on objective, measured the only way that really counts: play a
// whole match through the real TurnMachine and check the HUD is never empty on
// a human turn. Preference rules elsewhere (cooldown, caps, the Director's
// quality gate) are exactly the kind of thing that can quietly reintroduce a
// gap, so this asserts the outcome rather than the mechanism.
// ---------------------------------------------------------------------------

namespace {

struct Coverage {
	int humanTurns = 0;
	int covered = 0;
};

Coverage playMatchMeasuringCoverage(MissionEngine& engine, uint32_t seed, int maxCommands = 8000) {
	MatchState s = test::allInYard();
	RulesConfig rules;
	TurnMachine tm(s, rules);
	Rng rng(seed);
	Coverage cov;

	auto feed = [&](const std::vector<GameEvent>& evs) {
		for (const auto& e : evs) {
			engine.onEvent(e, s);
			if (e.type == ET::TURN_STARTED && e.player == 0 && s.phase != Phase::MatchOver) {
				cov.humanTurns++;
				if (!engine.activeInstances().empty()) cov.covered++;
			}
		}
	};

	engine.startMatch(0, seed);
	feed(tm.startMatch());
	for (int cmd = 0; cmd < maxCommands && s.phase != Phase::MatchOver; cmd++) {
		if (s.phase == Phase::AwaitingRoll) {
			feed(tm.roll(rng.dice()));
		} else if (s.phase == Phase::AwaitingMove) {
			auto opts = rules::legalMoves(s, s.current);
			if (opts.empty()) break;
			MoveOption o = BotBrain::choose(s, opts, rng);
			feed(tm.move(o.token, o.value));
		} else {
			break;
		}
	}
	return cov;
}

}  // namespace

TEST_CASE("always-on: a full match never starts a human turn without a live mission") {
	for (uint32_t seed : {3u, 11u, 29u, 57u, 101u}) {
		MissionEngine engine;
		REQUIRE(engine.loadFromJson(test::readContent("config/missions.json")).errors.empty());
		Coverage cov = playMatchMeasuringCoverage(engine, seed);
		CHECK(cov.humanTurns > 10);
		CHECK_MESSAGE(cov.covered == cov.humanTurns,
					  "seed " << seed << ": " << (cov.humanTurns - cov.covered) << " of " << cov.humanTurns << " human turns had no mission");
	}
}

TEST_CASE("always-on: with the full catalogue, rationing alone already covers every turn") {
	// Worth stating plainly: at 18 missions the offer conditions are broad enough that the weighted-random
	// strategy finds a candidate on practically every turn, so the guarantee is not what produces the number
	// above. It earns its keep in the two regimes below -- a thin catalogue, and the Director's quality gate.
	int gaps = 0;
	for (uint32_t seed : {3u, 11u, 29u, 57u, 101u}) {
		MissionEngine engine;
		MissionSettings s;
		s.alwaysOn = false;
		engine.setSettings(s);
		REQUIRE(engine.loadFromJson(test::readContent("config/missions.json")).errors.empty());
		Coverage cov = playMatchMeasuringCoverage(engine, seed);
		gaps += cov.humanTurns - cov.covered;
	}
	MESSAGE("rationed mode, full catalogue: " << gaps << " uncovered human turns across 5 matches");
	CHECK(gaps == 0);
}

TEST_CASE("always-on: with a thin catalogue it is the difference between cover and gaps") {
	// Two missions on long cooldowns -- the regime the guarantee exists for.
	const std::string thin = "{\"missions\":[" + alwaysMission("a", 2, 6) + "," + alwaysMission("b", 2, 6) + "]}";

	int rationedGaps = 0, alwaysOnGaps = 0, turns = 0;
	for (uint32_t seed : {3u, 11u, 29u, 57u, 101u}) {
		{
			MissionEngine engine;
			MissionSettings s;
			s.alwaysOn = false;
			engine.setSettings(s);
			REQUIRE(engine.loadFromJson(thin).errors.empty());
			Coverage cov = playMatchMeasuringCoverage(engine, seed);
			rationedGaps += cov.humanTurns - cov.covered;
			turns += cov.humanTurns;
		}
		{
			MissionEngine engine;
			REQUIRE(engine.loadFromJson(thin).errors.empty());
			Coverage cov = playMatchMeasuringCoverage(engine, seed);
			alwaysOnGaps += cov.humanTurns - cov.covered;
		}
	}
	MESSAGE("thin catalogue over " << turns << " human turns: rationed left " << rationedGaps << " uncovered, always-on left "
									<< alwaysOnGaps);
	CHECK(rationedGaps > 0);   // the gap the guarantee removes is real
	CHECK(alwaysOnGaps == 0);  // and it removes all of it
}
