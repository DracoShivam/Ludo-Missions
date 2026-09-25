#pragma once

#include <memory>

#include "Models/MatchState.h"

namespace lm {

struct EvalContext {
	const MatchState& state;
	int self = 0;
	int selfTurnIndex = 0;  // 1 = the human's first turn
};

class Condition {
public:
	virtual ~Condition() = default;
	virtual bool eval(const EvalContext& ctx) const = 0;
};
using ConditionPtr = std::shared_ptr<const Condition>;

}  // namespace lm
