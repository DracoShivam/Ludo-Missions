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

// ---------------------------------------------------------------------------
// Target solving: a mission may author a targetRange, and the Director picks
// the actual number per board so the difficulty lands on the player's band.
// ---------------------------------------------------------------------------

namespace {

const char* CAPTURE_RANGE = R"({"type":"count","event":"TOKEN_CAPTURED","where":{"player":"self"},"targetRange":[1,4]})";

// Solve the target the Director would serve for `mission` from `s`.
int solvedTargetFor(const CompiledMissionPtr& mission, const MatchState& s, double center, int rollouts = 240) {
	DirectorConfig cfg;
	cfg.rollouts = rollouts;
	cfg.simBudgetMs = 0;  // no time limit: this must be reproducible, not fast
	cfg.utility.minUtility = 0.0;
	DirectorStrategy d(cfg, RulesConfig{});
	d.difficulty().setCenter(center);
	Rng rng(9);
	EvalContext ctx{s, 0, 1};
	auto pick = d.choose({mission}, ctx, OfferMoment::TurnStart, OfferStats{}, rng);
	REQUIRE(pick.has_value());
	return d.solvedTarget(0);
}

}  // namespace

TEST_CASE("parser: targetRange compiles to the easiest variant and records the range") {
	auto m = compileOne(CAPTURE_RANGE, 5);
	CHECK(m->def.targetMin == 1);
	CHECK(m->def.targetMax == 4);
	// Without a Director the authored target is the floor, so an un-directed build still serves something winnable.
	CHECK(m->makeObjective()->target() == 1);
}

TEST_CASE("parser: a malformed targetRange is an error, not a silent default") {
	MissionEngine e;
	auto bad = e.loadFromJson(R"({"missions":[{"id":"x","title":"x","reward":{"coins":1},"turns":3,
		"objective":{"type":"count","event":"TOKEN_CAPTURED","targetRange":[4,1]}}]})");
	CHECK(bad.missions.empty());
	REQUIRE(bad.errors.size() == 1);
	CHECK(bad.errors[0].find("targetRange") != std::string::npos);
}

TEST_CASE("director: a richer board earns a harder target from the same mission") {
	auto mission = compileOne(CAPTURE_RANGE, 6);

	// One reachable enemy, nothing else on the board.
	auto lean = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	// Four of mine out, six enemies scattered just ahead of them.
	auto rich = test::makeState({{{14, 18, 22, 26}, {4, 8, 12, -1}, {30, 34, -1, -1}, {40, -1, -1, -1}}});

	int leanTarget = solvedTargetFor(mission, lean, 0.5);
	int richTarget = solvedTargetFor(mission, rich, 0.5);

	CHECK(leanTarget >= 1);
	CHECK(richTarget > leanTarget);
    MESSAGE("solved target: lean board " << leanTarget << ", rich board " << richTarget);
}

TEST_CASE("director: a lower difficulty centre asks for less on the same board") {
	auto mission = compileOne(CAPTURE_RANGE, 6);
	auto s = test::makeState({{{14, 18, 22, 26}, {4, 8, 12, -1}, {30, 34, -1, -1}, {40, -1, -1, -1}}});

	int easy = solvedTargetFor(mission, s, 0.85);  // wants a high completion probability
	int hard = solvedTargetFor(mission, s, 0.20);  // wants a low one
	CHECK(hard >= easy);
	MESSAGE("target at centre 0.85 = " << easy << ", at centre 0.20 = " << hard);
}

TEST_CASE("director: solving is deterministic for a fixed seed") {
	auto mission = compileOne(CAPTURE_RANGE, 6);
	auto s = test::makeState({{{14, 18, 22, -1}, {4, 8, -1, -1}, {30, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(solvedTargetFor(mission, s, 0.5) == solvedTargetFor(mission, s, 0.5));
}

TEST_CASE("director: the offered instance carries the solved target, not the authored floor") {
	MissionEngine engine;
	DirectorConfig cfg;
	cfg.rollouts = 200;
	cfg.simBudgetMs = 0;
	cfg.utility.minUtility = 0.0;
	auto strategy = std::make_unique<DirectorStrategy>(cfg, RulesConfig{});
	strategy->difficulty().setCenter(0.5);
	engine.setStrategy(std::move(strategy));

	auto r = engine.loadFromJson(std::string(R"({"missions":[{"id":"x","title":"Hunt","description":"Cut {target}",)") +
								 R"("reward":{"coins":10},"turns":6,"objective":)" + CAPTURE_RANGE + "}]}");
	REQUIRE(r.errors.empty());

	auto rich = test::makeState({{{14, 18, 22, 26}, {4, 8, 12, -1}, {30, 34, -1, -1}, {40, -1, -1, -1}}});
	engine.startMatch(0, 3);
	GameEvent e;
	e.type = GameEventType::TURN_STARTED;
	e.player = 0;
	auto updates = engine.onEvent(e, rich);

	REQUIRE(updates.size() == 1);
	CHECK(updates[0].kind == MissionUpdate::Kind::Offered);
	int target = updates[0].instance.target;
	CHECK(target >= 1);
	CHECK(target <= 4);
	// The rendered description must show the solved number, not the authored floor.
	CHECK(updates[0].instance.description == "Cut " + std::to_string(target));
	MESSAGE("offered target " << target << " -> \"" << updates[0].instance.description << "\"");
}

TEST_CASE("director: the A* stage cannot starve the Monte Carlo budget") {
	// Reproduces a real regression seen in the running game: with a large catalogue on an opening board, stage 1
	// consumed the whole 12ms decision budget, stage 2 got zero samples, every candidate scored P=0.00, and the
	// offer became arbitrary. The two stages now have separate clocks.
	MissionEngine engine;
	auto loaded = engine.loadFromJson(test::readContent("config/missions.json"));
	REQUIRE(loaded.errors.empty());

	std::vector<CompiledMissionPtr> candidates;
	for (const auto& m : loaded.missions) {
		if (std::find(m->def.moments.begin(), m->def.moments.end(), OfferMoment::TurnStart) != m->def.moments.end()) {
			candidates.push_back(m);
		}
	}
	REQUIRE(candidates.size() >= 10);

	DirectorConfig cfg;  // shipped budgets
	DirectorStrategy d(cfg, RulesConfig{});
	MatchState s = test::allInYard();  // the worst case for stage 1: every move is legal, nothing prunes
	EvalContext ctx{s, 0, 1};
	Rng rng(5);
	d.choose(candidates, ctx, OfferMoment::TurnStart, OfferStats{}, rng, false);

	int sampled = 0;
	double bestP = 0;
	for (const auto& ev : d.lastEvals()) {
		if (ev.runs >= 16) sampled++;
		bestP = std::max(bestP, ev.p);
	}
	MESSAGE("opening board, " << candidates.size() << " candidates: " << sampled << " reached >=16 runs in "
							  << d.lastElapsedMs() << "ms, best P=" << bestP);
	CHECK(sampled > 0);
	CHECK(bestP > 0.0);  // P=0 across the board is the signature of the starvation bug
}

TEST_CASE("director: the A* budget bounds stage 1 without dropping missions") {
	auto mission = starter("capture_in_3");
	DirectorConfig cfg;
	cfg.astarBudgetMs = 0;  // unbounded: the search actually runs
	DirectorStrategy unbounded(cfg, RulesConfig{});
	cfg.astarBudgetMs = 1;  // so tight that most candidates are skipped
	DirectorStrategy bounded(cfg, RulesConfig{});

	auto s = hunter();
	EvalContext ctx{s, 0, 1};
	std::vector<CompiledMissionPtr> cands{mission, mission, mission, mission, mission, mission};

	Rng r1(3), r2(3);
	unbounded.choose(cands, ctx, OfferMoment::TurnStart, OfferStats{}, r1, false);
	bounded.choose(cands, ctx, OfferMoment::TurnStart, OfferStats{}, r2, false);

	// Skipping a search must never turn into dropping the mission: unsearched candidates are Unknown, not Infeasible.
	for (const auto& ev : bounded.lastEvals()) CHECK_FALSE(ev.infeasible);
}

// ---------------------------------------------------------------------------
// Every offered mission must be one the player can actually do. A* removes the
// provably impossible; this floor removes the merely hopeless -- feasible in
// principle, never completed in any simulated future.
// ---------------------------------------------------------------------------

TEST_CASE("director: a hopeless mission is never offered when an achievable one exists") {
	// "Roll a 6 in two turns running" is feasible with chosen dice, so A* keeps it, but its real chance is
	// about 1/36. fitFloor alone would still let it clear minUtility on timeliness, so it used to get sampled.
	auto hopeless = starter("six_streak_2");
	auto achievable = starter("roll_six_now");
	auto s = test::allInYard();
	EvalContext ctx{s, 0, 1};

	DirectorConfig cfg;
	cfg.rollouts = 200;
	cfg.simBudgetMs = 0;
	cfg.astarBudgetMs = 0;

	int hopelessPicks = 0;
	double hopelessP = 1.0;
	for (uint32_t seed = 1; seed <= 25; seed++) {
		DirectorStrategy d(cfg, RulesConfig{});
		Rng rng(seed);
		auto pick = d.choose({hopeless, achievable}, ctx, OfferMoment::TurnStart, OfferStats{}, rng, false);
		hopelessP = std::min(hopelessP, d.lastEvals()[0].p);
		if (pick && *pick == 0) hopelessPicks++;
	}
	MESSAGE("six_streak_2 measured at P=" << hopelessP << "; picked " << hopelessPicks << " times in 25 decisions");
	CHECK(hopelessP < cfg.utility.minProbability);  // the premise: it really is hopeless here
	CHECK(hopelessPicks == 0);                      // and it is never served
}

TEST_CASE("director: when everything is hopeless, it serves the least hopeless one") {
	// The always-on guarantee still holds, but it should not pick arbitrarily among bad options.
	auto s = test::allInYard();
	EvalContext ctx{s, 0, 1};
	DirectorConfig cfg;
	cfg.rollouts = 200;
	cfg.simBudgetMs = 0;
	cfg.astarBudgetMs = 0;

	auto streak = starter("six_streak_2");   // ~1/36
	auto relentless = starter("relentless");  // needs captures that cannot happen from the yard

	DirectorStrategy d(cfg, RulesConfig{});
	Rng rng(4);
	auto pick = d.choose({relentless, streak}, ctx, OfferMoment::TurnStart, OfferStats{}, rng, true);
	REQUIRE(pick.has_value());  // mustOffer: something is served
	const auto& evals = d.lastEvals();
	CHECK(evals[*pick].p >= evals[1 - *pick].p);  // and it is the more achievable of the two
	MESSAGE("both hopeless: served " << evals[*pick].id << " at P=" << evals[*pick].p);
}
