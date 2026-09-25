#include "doctest/doctest.h"

#include "Controllers/Logic/Missions/EventFilter.h"
#include "Controllers/Logic/Missions/MissionEngine.h"
#include "Controllers/Logic/Missions/TextTemplate.h"
#include "TestHelpers.h"

using namespace lm;

static MissionParseResult parse(const std::string& json) {
	MissionEngine e;
	return e.loadFromJson(json);
}

static std::string one(const std::string& missionBody) {
	return R"({"missions":[{"id":"m","title":"T","reward":{"coins":5},"turns":2,)" + missionBody + "}]}";
}

TEST_CASE("parser: starter missions.json loads cleanly") {
	auto r = parse(test::readContent("config/missions.json"));
	for (auto& e : r.errors) MESSAGE(e);
	for (auto& w : r.warnings) MESSAGE(w);
	CHECK(r.errors.empty());
	CHECK(r.warnings.empty());
	CHECK(r.missions.size() == 18);  // tracks Content/config/missions.json
}

TEST_CASE("parser: errors are per mission and name the id") {
	auto r = parse(R"({"missions":[
		{"title":"no id","reward":{"coins":1},"turns":1,"objective":{"type":"count","event":"DICE_ROLLED","target":1}},
		{"id":"bad_cond","title":"x","reward":{"coins":1},"turns":1,"offerWhen":{"type":"nope"},"objective":{"type":"count","event":"DICE_ROLLED","target":1}},
		{"id":"bad_field","title":"x","reward":{"coins":1},"turns":1,"objective":{"type":"count","event":"DICE_ROLLED","where":{"playr":"self"},"target":1}},
		{"id":"ok","title":"x","reward":{"coins":1},"turns":1,"objective":{"type":"count","event":"DICE_ROLLED","target":1}},
		{"id":"ok","title":"dup","reward":{"coins":1},"turns":1,"objective":{"type":"count","event":"DICE_ROLLED","target":1}},
		{"id":"off","enabled":false,"title":"x","reward":{"coins":1},"turns":1,"objective":{"type":"count","event":"DICE_ROLLED","target":1}}
	]})");
	CHECK(r.missions.size() == 1);
	REQUIRE(r.errors.size() == 4);
	CHECK(r.errors[0].find("missing \"id\"") != std::string::npos);
	CHECK(r.errors[1].find("'bad_cond'") != std::string::npos);
	CHECK(r.errors[1].find("unknown condition type 'nope'") != std::string::npos);
	CHECK(r.errors[2].find("unknown field 'playr'") != std::string::npos);
	CHECK(r.errors[3].find("duplicate") != std::string::npos);
}

TEST_CASE("parser: unknown keys warn, syntax errors report line") {
	auto r = parse(one(R"("trun":3,"objective":{"type":"count","event":"DICE_ROLLED","target":1,"extra":1})"));
	CHECK(r.errors.empty());
	CHECK(r.warnings.size() == 2);
	auto bad = parse("{\n\"missions\": [\n  {,}\n]}");
	REQUIRE(bad.errors.size() == 1);
	CHECK(bad.errors[0].find("line 3") != std::string::npos);
}

TEST_CASE("parser: required params and moments") {
	CHECK(parse(one(R"("objective":{"type":"count","event":"DICE_ROLLED"})")).errors.size() == 1);   // target missing
	CHECK(parse(one(R"("objective":{"type":"count","event":"NOPE","target":1})")).errors.size() == 1);
	CHECK(parse(one(R"("moments":["later"],"objective":{"type":"count","event":"DICE_ROLLED","target":1})")).errors.size() == 1);
	auto ok = parse(one(R"("moments":["afterRoll"],"objective":{"type":"count","event":"DICE_ROLLED","target":1})"));
	REQUIRE(ok.missions.size() == 1);
	CHECK(ok.missions[0]->def.moments.size() == 1);
	CHECK(ok.missions[0]->def.moments[0] == OfferMoment::AfterRoll);
}

TEST_CASE("event filter: scalars, operators, roles") {
	Params p;
	p.values["player"] = std::string("enemy");
	auto opSpec = std::make_shared<Spec>();
	opSpec->params.values["gte"] = (int64_t) 5;
	p.children["value"] = opSpec;
	EventFilter f;
	std::string err;
	REQUIRE(EventFilter::compile(p, f, err));
	CHECK(f.matches(test::rolled(1, 5), 0));
	CHECK_FALSE(f.matches(test::rolled(0, 6), 0));  // self, not enemy
	CHECK_FALSE(f.matches(test::rolled(2, 4), 0));

	Params in;
	in.arrays["value"];  // arrays at top-level are invalid
	EventFilter g;
	CHECK_FALSE(EventFilter::compile(in, g, err));

	Params inOp;
	auto s = std::make_shared<Spec>();
	s->params.arrays["in"] = {(int64_t) 1, (int64_t) 6};
	inOp.children["value"] = s;
	EventFilter h;
	REQUIRE(EventFilter::compile(inOp, h, err));
	CHECK(h.matches(test::rolled(0, 6), 0));
	CHECK_FALSE(h.matches(test::rolled(0, 3), 0));
}

TEST_CASE("text template") {
	CHECK(renderTemplate("Move {target} in {turns} ({progress}) {x}", {{"target", 25}, {"turns", 3}, {"progress", 7}}) == "Move 25 in 3 (7) {x}");
}

TEST_CASE("parser: a mission may grant a power alongside coins") {
	auto r = parse(R"({"missions":[{"id":"x","title":"x","reward":{"coins":50,"power":"kick"},"turns":3,
		"objective":{"type":"count","event":"TOKEN_CAPTURED","target":1}}]})");
	REQUIRE(r.errors.empty());
	REQUIRE(r.missions.size() == 1);
	CHECK(r.missions[0]->def.rewardCoins == 50);
	CHECK(r.missions[0]->def.rewardPower == "kick");
}

TEST_CASE("parser: a reward power id is carried through verbatim") {
	// The mission parser hands the id onward exactly as it hands on a coin amount; whether the
	// power exists is the power catalogue's business, checked once at load rather than per mission.
	auto r = parse(R"({"missions":[{"id":"x","title":"x","reward":{"coins":50,"power":"teleport"},"turns":3,
		"objective":{"type":"count","event":"TOKEN_CAPTURED","target":1}}]})");
	REQUIRE(r.errors.empty());
	REQUIRE(r.missions.size() == 1);
	CHECK(r.missions[0]->def.rewardPower == "teleport");
}

TEST_CASE("parser: missions without a power reward still parse") {
	auto r = parse(test::readContent("config/missions.json"));
	REQUIRE(r.errors.empty());
	int withPower = 0;
	for (const auto& m : r.missions) {
		if (!m->def.rewardPower.empty()) withPower++;
	}
	MESSAGE(withPower << " of " << r.missions.size() << " shipped missions grant a power");
	CHECK(withPower > 0);
	CHECK(withPower < (int) r.missions.size());  // powers stay scarce
}
