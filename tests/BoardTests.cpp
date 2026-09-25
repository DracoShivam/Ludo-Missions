#include <cmath>

#include "doctest/doctest.h"

#include "Controllers/Logic/BoardQueries.h"
#include "Models/BoardLayout.h"
#include "TestHelpers.h"

using namespace lm;

TEST_CASE("layout: adjacent track cells are one grid step apart") {
	for (int i = 0; i < TRACK_LEN; i++) {
		GridPos a = board::trackGrid(i);
		GridPos b = board::trackGrid((i + 1) % TRACK_LEN);
		float cheb = std::max(std::fabs(a.col - b.col), std::fabs(a.row - b.row));
		CHECK_MESSAGE(cheb == doctest::Approx(1.0), "cells " << i << " and " << i + 1);
	}
}

TEST_CASE("layout: last track cell is orthogonally adjacent to the home lane entrance") {
	for (int c = 0; c < 4; c++) {
		GridPos a = board::trackGrid(board::globalCell(c, 50));
		GridPos b = board::homeLaneGrid(c, 0);
		CHECK(std::fabs(a.col - b.col) + std::fabs(a.row - b.row) == doctest::Approx(1.0));
	}
}

TEST_CASE("layout: global cells, start cells and safe cells") {
	CHECK(board::globalCell(RED, 0) == 0);
	CHECK(board::globalCell(GREEN, 0) == 13);
	CHECK(board::globalCell(BLUE, 20) == (39 + 20) % 52);
	CHECK(board::globalCell(RED, 51) == -1);
	CHECK(board::globalCell(RED, -1) == -1);
	for (int c = 0; c < 4; c++) {
		CHECK(board::isSafeCell(board::startCell(c)));
		CHECK(board::isSafeCell(board::startCell(c) + 8));
	}
	CHECK_FALSE(board::isSafeCell(17));
}

TEST_CASE("queries: enemy ahead / behind") {
	// Hunter scenario: RED at 14 (global 14), GREEN at 4 (global 17) -> enemy 3 ahead.
	auto s = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(queries::enemyAheadDistance(s, 0, 0, 1, 6) == 3);
	CHECK(queries::anyEnemyAhead(s, 0, 3, 3));
	CHECK_FALSE(queries::anyEnemyAhead(s, 0, 4, 6));
	// From GREEN's perspective RED is 3 behind.
	CHECK(queries::enemyBehindDistance(s, 1, 0, 1, 6) == 3);
	// Two enemies on the same cell protect each other.
	auto s2 = test::makeState({{{14, -1, -1, -1}, {4, -1, -1, -1}, {43, -1, -1, -1}, {-1, -1, -1, -1}}});  // YELLOW 43 -> (26+43)%52=17
	CHECK(queries::enemyAheadDistance(s2, 0, 0, 1, 6) == 0);
}

TEST_CASE("queries: token counts and race rank") {
	auto s = test::makeState({{{-1, 5, 53, 56}, {10, 10, -1, -1}, {-1, -1, -1, -1}, {-1, -1, -1, -1}}});
	CHECK(queries::countTokens(s, 0, queries::Zone::Yard) == 1);
	CHECK(queries::countTokens(s, 0, queries::Zone::Track) == 1);
	CHECK(queries::countTokens(s, 0, queries::Zone::HomeLane) == 1);
	CHECK(queries::countTokens(s, 0, queries::Zone::Finished) == 1);
	CHECK(queries::countTokens(s, 0, queries::Zone::OutOfYard) == 3);
	CHECK(queries::countEnemyTokens(s, 0, queries::Zone::Track) == 2);
	CHECK(queries::raceRank(s, 0) == 1);
	CHECK(queries::raceRank(s, 2) == 3);
}
