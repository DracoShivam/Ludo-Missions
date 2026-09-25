#include "Models/BoardLayout.h"

namespace lm {
namespace board {

// Verified against chaupar ludo_board.prefab (chaupar box(i+1) == our global cell i).
static const GridPos TRACK_GRID[TRACK_LEN] = {
	{6, 1},  {6, 2},  {6, 3},  {6, 4},  {6, 5},  {5, 6},  {4, 6},  {3, 6},  {2, 6},  {1, 6},  {0, 6},  {0, 7},  {0, 8},
	{1, 8},  {2, 8},  {3, 8},  {4, 8},  {5, 8},  {6, 9},  {6, 10}, {6, 11}, {6, 12}, {6, 13}, {6, 14}, {7, 14}, {8, 14},
	{8, 13}, {8, 12}, {8, 11}, {8, 10}, {8, 9},  {9, 8},  {10, 8}, {11, 8}, {12, 8}, {13, 8}, {14, 8}, {14, 7}, {14, 6},
	{13, 6}, {12, 6}, {11, 6}, {10, 6}, {9, 6},  {8, 5},  {8, 4},  {8, 3},  {8, 2},  {8, 1},  {8, 0},  {7, 0},  {6, 0},
};

static const GridPos HOME_LANE_GRID[NUM_PLAYERS][5] = {
	{{7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 5}},
	{{1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}},
	{{7, 13}, {7, 12}, {7, 11}, {7, 10}, {7, 9}},
	{{13, 7}, {12, 7}, {11, 7}, {10, 7}, {9, 7}},
};

static const GridPos YARD_CENTER[NUM_PLAYERS] = {{2.5f, 2.5f}, {2.5f, 11.5f}, {11.5f, 11.5f}, {11.5f, 2.5f}};
static const GridPos FINISH_SPOT[NUM_PLAYERS] = {{7, 6.4f}, {6.4f, 7}, {7, 7.6f}, {7.6f, 7}};
static const int SAFE_CELLS[] = {0, 8, 13, 21, 26, 34, 39, 47};
static const float YARD_SPOT_OFFSET = 0.8f;

int startCell(int color) {
	return color * 13;
}

int globalCell(int color, int progress) {
	if (progress < 0 || progress > LAST_TRACK_PROGRESS) {
		return -1;
	}
	return (startCell(color) + progress) % TRACK_LEN;
}

bool isSafeCell(int cell) {
	for (int s : SAFE_CELLS) {
		if (s == cell) {
			return true;
		}
	}
	return false;
}

GridPos trackGrid(int cell) {
	return TRACK_GRID[((cell % TRACK_LEN) + TRACK_LEN) % TRACK_LEN];
}

GridPos homeLaneGrid(int color, int laneIdx) {
	return HOME_LANE_GRID[color][laneIdx];
}

GridPos yardCenterGrid(int color) {
	return YARD_CENTER[color];
}

GridPos yardSpotGrid(int color, int tokenIdx) {
	GridPos c = YARD_CENTER[color];
	float dx = (tokenIdx % 2 == 0) ? -YARD_SPOT_OFFSET : YARD_SPOT_OFFSET;
	float dy = (tokenIdx < 2) ? -YARD_SPOT_OFFSET : YARD_SPOT_OFFSET;
	return {c.col + dx, c.row + dy};
}

GridPos finishGrid(int color) {
	return FINISH_SPOT[color];
}

GridPos gridForToken(int color, int tokenIdx, int progress) {
	if (progress == IN_YARD) {
		return yardSpotGrid(color, tokenIdx);
	}
	if (progress >= FINISHED) {
		return finishGrid(color);
	}
	if (progress >= HOME_LANE_FIRST) {
		return homeLaneGrid(color, progress - HOME_LANE_FIRST);
	}
	return trackGrid(globalCell(color, progress));
}

}  // namespace board
}  // namespace lm
