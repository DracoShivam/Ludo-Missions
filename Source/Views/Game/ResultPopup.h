#pragma once

#include <string>
#include <vector>

#include "axmol.h"

namespace lm {

struct ResultData {
	std::vector<int> ranking;
	std::vector<std::string> names;
	int self = 0;
	int missionsCompleted = 0;
	int coinsEarned = 0;
};

// Modal end-of-match popup. Buttons publish UiResultClosed.
class ResultPopup : public ax::Node {
public:
	static ResultPopup* create(const ResultData& data);
	bool initWith(const ResultData& data);
};

}  // namespace lm
