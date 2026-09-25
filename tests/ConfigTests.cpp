#include "doctest/doctest.h"

#include "Controllers/Logic/ConfigParser.h"

using namespace lm;

TEST_CASE("config: empty object gives defaults") {
	auto r = parseGameConfig("{}");
	CHECK(r.errors.empty());
	CHECK(r.config.rules.threeSixesForfeit);
	CHECK(r.config.missions.maxActive == 3);
	CHECK(r.config.timing.tokenStep == doctest::Approx(0.18));
	CHECK(r.config.director.rollouts == 96);
}

TEST_CASE("config: overrides are applied") {
	auto r = parseGameConfig(R"({"rules":{"captureBonusRoll":false},"timing":{"tokenStep":0.5},
		"missions":{"maxActive":2,"offersPerMoment":{"afterRoll":0}},"director":{"enabled":false,"utility":{"minUtility":0.3}}})");
	CHECK(r.errors.empty());
	CHECK_FALSE(r.config.rules.captureBonusRoll);
	CHECK(r.config.timing.tokenStep == doctest::Approx(0.5));
	CHECK(r.config.missions.maxActive == 2);
	CHECK(r.config.missions.offersPerAfterRoll == 0);
	CHECK_FALSE(r.config.director.enabled);
	CHECK(r.config.director.utility.minUtility == doctest::Approx(0.3));
}

TEST_CASE("config: syntax error reports line and column") {
	auto r = parseGameConfig("{\n  \"rules\": {\n    \"x\": ,\n  }\n}");
	REQUIRE(r.errors.size() == 1);
	CHECK(r.errors[0].find("line 3") != std::string::npos);
}

TEST_CASE("config: startProgress validated") {
	auto ok = parseGameConfig(R"({"debug":{"startProgress":[[14,-1,-1,-1],[4,-1,-1,-1],[-1,-1,-1,-1],[-1,-1,-1,-1]]}})");
	CHECK(ok.errors.empty());
	REQUIRE(ok.config.debug.startProgress.size() == 4);
	CHECK(ok.config.debug.startProgress[1][0] == 4);
	auto bad = parseGameConfig(R"({"debug":{"startProgress":[[1,2]]}})");
	CHECK(bad.errors.size() == 1);
	CHECK(bad.config.debug.startProgress.empty());
}
