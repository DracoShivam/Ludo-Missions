#pragma once

#include "Models/Types.h"

namespace lm {

// Grid coordinates on the 15x15 board, row 0 at the bottom, cell centres at integers.
struct GridPos {
	float col = 0;
	float row = 0;
};

namespace board {

int startCell(int color);                 // 0, 13, 26, 39
int globalCell(int color, int progress);  // valid for progress 0..50 only, else -1
bool isSafeCell(int globalCell);          // 4 starts + 4 stars

GridPos trackGrid(int globalCell);
GridPos homeLaneGrid(int color, int laneIdx);  // laneIdx 0..4
GridPos yardSpotGrid(int color, int tokenIdx);
GridPos yardCenterGrid(int color);
GridPos finishGrid(int color);
// Where a token with `progress` is drawn (yard spot / track / lane / finish).
GridPos gridForToken(int color, int tokenIdx, int progress);

}  // namespace board
}  // namespace lm
