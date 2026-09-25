#include "doctest/doctest.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "Controllers/Logic/Missions/MissionEngine.h"
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
	CHECK(u[0].instance.description == "Move 25 cells within 3 turns (12/25)");
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
	Harness h("{\"missions\":[" + alwaysMission("a", 1, 2) + "]}");
	REQUIRE(has(h.turnStart(), K::Offered));  // turn 1
	REQUIRE(has(h.turnEnd(), K::Failed));     // resolved in turn 1 -> eligible at turn 4
	CHECK(h.turnStart().empty());             // 2
	h.turnEnd();
	CHECK(h.turnStart().empty());  // 3
	h.turnEnd();
	CHECK(has(h.turnStart(), K::Offered));  // 4
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
