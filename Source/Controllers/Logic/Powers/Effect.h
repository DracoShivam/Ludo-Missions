#pragma once

#include <memory>
#include <vector>

#include "Models/GameEvent.h"
#include "Models/MatchState.h"
#include "Models/PowerDef.h"

namespace lm {

// What a power is aimed at. {-1, -1} for a power that needs no target; token == -1 when the
// target is a whole player rather than one of their tokens.
struct PowerTarget {
	int player = -1;
	int token = -1;
	bool operator==(const PowerTarget& o) const { return player == o.player && token == o.token; }
	bool operator!=(const PowerTarget& o) const { return !(*this == o); }
};

struct EffectContext {
	const MatchState& state;
	int user = 0;
};

// One kind of power effect. Effects are stateless and shared: the catalogue compiles each
// one once and every use of that power runs the same instance.
//
// `targets` is the single source of truth for what may be aimed at. The board highlights
// exactly what it returns, and TargetingSession accepts exactly what it returns -- which is
// what stops "the UI shows one thing, the rules accept another".
class IPowerEffect {
public:
	virtual ~IPowerEffect() = default;
	virtual TargetKind targetKind() const = 0;
	virtual std::vector<PowerTarget> targets(const EffectContext& ctx) const = 0;
	// Mutates `s`. Only ever called with a target that `targets()` returned.
	virtual std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const = 0;
};

using PowerEffectPtr = std::shared_ptr<const IPowerEffect>;

}  // namespace lm
