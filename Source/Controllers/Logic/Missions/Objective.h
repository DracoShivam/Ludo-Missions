#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "Controllers/Logic/Missions/Condition.h"
#include "Models/GameEvent.h"

namespace lm {

struct ObjectiveResult {
	bool changed = false;
	bool completed = false;
	bool failed = false;
};

// Runtime objective (state inside). A fresh one is made per offer from an ObjectiveFactory.
class Objective {
public:
	virtual ~Objective() = default;
	virtual void begin(const EvalContext&, int turns) {}
	virtual ObjectiveResult onEvent(const GameEvent& e, const EvalContext& ctx) = 0;  // every event, incl. TURN_ENDED
	virtual ObjectiveResult onSelfTurnEnded(int turnsLeftAfter) { return {}; }      // after the decrement
	virtual bool completesOnWindowEnd() const { return false; }                     // avoid => true
	virtual bool alreadySatisfied(const EvalContext&) const { return false; }       // state => cond.eval
	virtual int progress() const = 0;
	virtual int target() const = 0;
	// Target solving: a mission may author a targetRange instead of a fixed target, and the Director retunes it per
	// offer so the difficulty lands on the player's band. No-op for shapes with no meaningful target (avoid/state).
	virtual void setTarget(int t) {}
	virtual bool targetIsTunable() const { return false; }
	// Mission Director support
	virtual std::unique_ptr<Objective> clone() const = 0;
	virtual uint64_t stateKey() const { return (uint64_t) progress(); }
	virtual int minTurnsHint(const EvalContext&, int turnsLeft) const { return 0; }  // ADMISSIBLE; 0 is always safe
	// A* only: true if the objective provably completes at window end when all bots are frozen (e.g. avoid enemy captures).
	virtual bool completesWhenBotsFrozen() const { return false; }
};

using ObjectiveFactory = std::function<std::unique_ptr<Objective>()>;

}  // namespace lm
