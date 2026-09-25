#include "doctest/doctest.h"

#include "Controllers/Logic/Rules.h"
#include "TestHelpers.h"

using namespace lm;

TEST_CASE("rules: target progress") {
	CHECK(rules::targetProgress(-1, 6) == 0);
	CHECK(rules::targetProgress(-1, 5) == INVALID_PROGRESS);
	CHECK(rules::targetProgress(53, 3) == 56);
	CHECK(rules::targetProgress(53, 4) == INVALID_PROGRESS);
	CHECK(rules::targetProgress(56, 1) == INVALID_PROGRESS);
	CHECK(rules::targetProgress(48, 5) == 53);
}

TEST_CASE("rules: captures") {
	// Single enemy on non-safe cell 17 -> capturable.
	auto s = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	auto v = rules::capturableAt(s, 0, 17);
	REQUIRE(v.has_value());
	CHECK(v->first == 1);
	CHECK(v->second == 0);
	// Safe cell (GREEN start = global 13) -> no capture.
	auto s2 = test::makeState({{{10, -1, -1, -1}, {0, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK_FALSE(rules::capturableAt(s2, 0, 13).has_value());
	// Two enemy tokens on the cell -> no capture.
	auto s3 = test::makeState({{{14, -1, -1, -1}, {4, 4, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK_FALSE(rules::capturableAt(s3, 0, 17).has_value());
	// Home lane never captures.
	CHECK_FALSE(rules::capturableAt(s, 0, 52).has_value());
}

TEST_CASE("rules: legal moves dedup roll values and auto move") {
	auto s = test::makeState({{{10, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.pendingRolls = {4, 4};
	auto m = rules::legalMoves(s, 0);
	REQUIRE(m.size() == 1);
	CHECK(m[0].to == 14);
	CHECK(rules::autoMove(s).has_value());

	s.pendingRolls = {6, 4};  // token0 by 6 or 4, yard tokens by 6 -> several options
	m = rules::legalMoves(s, 0);
	CHECK(m.size() == 5);
	CHECK_FALSE(rules::autoMove(s).has_value());
	auto vals = rules::legalValuesForToken(s, 0, 0);
	CHECK(vals == std::vector<int>{4, 6});
}

TEST_CASE("rules: own tokens can stack") {
	auto s = test::makeState({{{10, 14, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	s.pendingRolls = {4};
	auto m = rules::legalMoves(s, 0);
	CHECK(m.size() == 2);
	CHECK_FALSE(m[0].captures);
}
