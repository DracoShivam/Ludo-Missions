#pragma once

#include <vector>

#include "axmol.h"

namespace lm {

// Row of small dice chips shown above a token when several roll values are legal for it. Tap -> UiRollChosen.
class RollChoiceView : public ax::Node {
public:
	static RollChoiceView* create(int player, int token, const std::vector<int>& values);
	bool initWith(int player, int token, const std::vector<int>& values);
};

}  // namespace lm
