#pragma once

#include "axmol.h"
#include "Models/BoardLayout.h"
#include "Views/Common/UiConfig.h"

namespace lm::ui {

// Grid -> BoardView-local position (BoardView is centred on grid (7,7)).
inline ax::Vec2 gridToLocal(GridPos g) {
	return ax::Vec2((g.col - 7.f) * CELL, (g.row - 7.f) * CELL);
}

}  // namespace lm::ui
