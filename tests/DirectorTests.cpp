#include <chrono>

#include "doctest/doctest.h"

#include "Controllers/Logic/Missions/Director/DirectorStrategy.h"
#include "Controllers/Logic/Missions/Director/FeasibilitySearch.h"
#include "Controllers/Logic/Missions/Director/RolloutSimulator.h"
#include "Controllers/Logic/Missions/Director/UtilityScorer.h"
#include "Controllers/Logic/Missions/MissionEngine.h"
#include "TestHelpers.h"

using namespace lm;
using V = SearchResult::Verdict;

namespace {

CompiledMissionPtr compileOne(const std::string& objective, int turns, const std::string& extra = "") {
	MissionEngine e;
	std::string json = R"({"missions":[{"id":"x","title":"x","reward":{"coins":1},"turns":)" + std::to_string(turns) + extra +
					   R"(,"objective":)" + objective + "}]}";
	auto r = e.loadFromJson(json);
	REQUIRE(r.errors.empty());
	return r.missions[0];
}

CompiledMissionPtr starter(const std::string& id) {
	MissionEngine e;
	auto r = e.loadFromJson(test::readContent("config/missions.json"));
	for (auto& m : r.missions)
		if (m->def.id == id) return m;
	FAIL("missing starter " << id);
	return nullptr;
}

const char* CAPTURE = R"({"type":"count","event":"TOKEN_CAPTURED","where":{"player":"self"},"target":1})";
const char* FINISH = R"({"type":"count","event":"TOKEN_FINISHED","where":{"player":"self"},"target":1})";
const char* AVOID = R"({"type":"avoid","event":"TOKEN_CAPTURED","where":{"victimPlayer":"self"}})";

MatchState hunter() {
	return test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
}

}  // namespace

TEST_CASE("A*: Hunter scenario is feasible this turn") {
	auto r = feasibilitySearch(*starter("capture_in_3"), hunter(), 0, 1, RulesConfig{}, 4000);
	CHECK(r.verdict == V::Feasible);
	CHECK(r.minTurns == 0);
}

TEST_CASE("A*: 20 cells in one turn is infeasible (three-sixes caps a turn at 17)") {
	// RED at 5 (global 5); GREEN at progress 12 = global 25 (not safe), exactly 20 ahead.
	auto s = test::makeState({{{5, -1, -1, -1}, {12, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto r1 = feasibilitySearch(*compileOne(CAPTURE, 1), s, 0, 1, RulesConfig{}, 20000);
	CHECK(r1.verdict == V::Infeasible);
	auto r2 = feasibilitySearch(*compileOne(CAPTURE, 2), s, 0, 1, RulesConfig{}, 20000);
	CHECK(r2.verdict == V::Feasible);
	CHECK(r2.minTurns == 1);
}

TEST_CASE("A*: finishing from progress 5 needs 3 turns") {
	auto s = test::makeState({{{5, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto t0 = std::chrono::steady_clock::now();
	auto r2 = feasibilitySearch(*compileOne(FINISH, 2), s, 0, 1, RulesConfig{}, 50000);
	auto r3 = feasibilitySearch(*compileOne(FINISH, 3), s, 0, 1, RulesConfig{}, 50000);
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	MESSAGE("finish searches: " << r2.expansions << " + " << r3.expansions << " expansions, " << ms << " ms");
	CHECK(r2.verdict == V::Infeasible);
	CHECK(r3.verdict == V::Feasible);
	CHECK(r3.minTurns == 2);
}

TEST_CASE("A*: avoid is always feasible; tiny budget is Unknown; deterministic") {
	auto a = feasibilitySearch(*compileOne(AVOID, 3), hunter(), 0, 1, RulesConfig{}, 4000);
	CHECK(a.verdict == V::Feasible);
	auto s = test::makeState({{{5, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto u = feasibilitySearch(*compileOne(FINISH, 3), s, 0, 1, RulesConfig{}, 10);
	CHECK(u.verdict == V::Unknown);
	auto x = feasibilitySearch(*compileOne(FINISH, 3), s, 0, 1, RulesConfig{}, 3000);
	auto y = feasibilitySearch(*compileOne(FINISH, 3), s, 0, 1, RulesConfig{}, 3000);
	CHECK(x.expansions == y.expansions);
}

TEST_CASE("Monte Carlo: probabilities") {
	Rng rng(123);
	auto six = starter("roll_six_now");
	int ok = 0, n = 3000;
	for (int i = 0; i < n; i++) ok += simulateMissionOnce(*six, test::allInYard(), 0, 1, RulesConfig{}, rng) ? 1 : 0;
	CHECK((double) ok / n == doctest::Approx(1.0 / 6).epsilon(0.18));

	auto strike = starter("strike_now");
	auto s = hunter();
	s.pendingRolls = {3};
	s.phase = Phase::AwaitingMove;
	ok = 0;
	for (int i = 0; i < 50; i++) ok += simulateMissionOnce(*strike, s, 0, 1, RulesConfig{}, rng) ? 1 : 0;
	CHECK(ok == 50);

	auto survive = starter("survive_4");
	auto safe = test::makeState({{{52, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	ok = 0;
	for (int i = 0; i < 50; i++) ok += simulateMissionOnce(*survive, safe, 0, 1, RulesConfig{}, rng) ? 1 : 0;
	CHECK(ok == 50);
}

TEST_CASE("utility: fit, repeat penalty") {
	MissionDef d;
	d.id = "a";
	d.weight = 10;
	UtilityConfig u;
	OfferStats st;
	double atCenter = missionUtility(d, 0.5, 1, 0.5, 0.15, u, st);
	double far = missionUtility(d, 0.95, 1, 0.5, 0.15, u, st);
	CHECK(atCenter > far);
	CHECK(missionUtility(d, 0.5, 0, 0.5, 0.15, u, st) > atCenter);  // timely
	st.lastOfferedId = "a";
	st.offersThisMatch["a"] = 1;
	CHECK(missionUtility(d, 0.5, 1, 0.5, 0.15, u, st) == doctest::Approx(atCenter * 0.5 * 0.5));
}

TEST_CASE("difficulty tracker steps, clamps, behind bias") {
	DifficultyConfig c;
	DifficultyTracker t(c);
	t.onResolved(true);
	CHECK(t.center() == doctest::Approx(0.45));
	for (int i = 0; i < 50; i++) t.onResolved(false);
	CHECK(t.center() == doctest::Approx(0.85));
	t.reset();
	CHECK(t.effectiveCenter(4) == doctest::Approx(0.6));
	CHECK(t.effectiveCenter(2) == doctest::Approx(0.5));
}

TEST_CASE("director: gate, never infeasible, afterRoll strike, determinism") {
	DirectorConfig cfg;
	cfg.simBudgetMs = 0;
	cfg.rollouts = 48;
	MissionEngine engine;
	engine.loadFromJson(test::readContent("config/missions.json"));
	auto defs = engine.definitions();

	// Infeasible candidates are never picked: 20-cell capture in 1 turn.
	auto s = test::makeState({{{5, -1, -1, -1}, {12, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto capture1 = compileOne(CAPTURE, 1);
	DirectorStrategy d(cfg, RulesConfig{});
	Rng rng(5);
	EvalContext ctx{s, 0, 1};
	CHECK_FALSE(d.choose({capture1}, ctx, OfferMoment::TurnStart, OfferStats{}, rng).has_value());
	CHECK(d.lastEvals()[0].infeasible);
	MESSAGE(d.lastDecisionLog());

	// Gate: impossible-to-satisfy threshold -> no offer
	DirectorConfig gated = cfg;
	gated.utility.minUtility = 100;
	DirectorStrategy g(gated, RulesConfig{});
	EvalContext c2{test::allInYard(), 0, 1};
	CHECK_FALSE(g.choose({starter("roll_six_now")}, c2, OfferMoment::TurnStart, OfferStats{}, rng).has_value());

	// afterRoll with a capturing roll pending -> strike_now served through the engine
	MissionEngine e2;
	MissionSettings ms;
	e2.setSettings(ms);
	e2.loadFromJson(test::readContent("config/missions.json"));
	e2.setStrategy(std::make_unique<DirectorStrategy>(cfg, RulesConfig{}));
	e2.startMatch(0, 9);
	auto h = hunter();
	e2.onEvent(test::ev(GameEventType::TURN_STARTED, 0), h);
	h.pendingRolls = {3};
	h.phase = Phase::AwaitingMove;
	auto u = e2.onEvent(test::rolled(0, 3), h);
	bool strike = false;
	for (auto& x : u) strike |= (x.kind == MissionUpdate::Kind::Offered && x.instance.id == "strike_now");
	CHECK(strike);

	// Determinism
	DirectorStrategy a(cfg, RulesConfig{}), b(cfg, RulesConfig{});
	Rng ra(77), rb(77);
	EvalContext c3{hunter(), 0, 1};
	CHECK(a.choose(defs, c3, OfferMoment::TurnStart, OfferStats{}, ra) == b.choose(defs, c3, OfferMoment::TurnStart, OfferStats{}, rb));
}

TEST_CASE("director perf (informational)") {
	DirectorConfig cfg;
	cfg.simBudgetMs = 0;
	MissionEngine engine;
	engine.loadFromJson(test::readContent("config/missions.json"));
	auto s = test::makeState({{{10, 30, -1, -1}, {5, 20, -1, -1}, {40, -1, -1, -1}, {3, 15, 25, -1}}});
	DirectorStrategy d(cfg, RulesConfig{});
	Rng rng(3);
	EvalContext ctx{s, 0, 3};
	d.choose(engine.definitions(), ctx, OfferMoment::TurnStart, OfferStats{}, rng);
	MESSAGE("director full eval: " << d.lastElapsedMs() << " ms | " << d.lastDecisionLog());
}
