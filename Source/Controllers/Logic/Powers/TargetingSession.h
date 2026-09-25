#pragma once

#include <string>
#include <vector>

#include "Controllers/Logic/Powers/Effect.h"

namespace lm {

// The "player has armed a power and is choosing what to aim it at" state, as a pure object.
//
// This lived as a raw member inside GameController and was the source of the bug that made
// the game unplayable: an illegal tap left the mode armed forever, and every later tap on
// any token was swallowed into it. Two rules prevent that class of bug here, and both are
// unit-tested:
//
//   1. Any tap resolves the session. Applied on a legal target, Cancelled on anything else.
//      A session never survives a tap.
//   2. cancel() is idempotent and always available, so there is no state the caller can be
//      stuck in and no affordance that can become unreachable.
class TargetingSession {
public:
	enum class TapResult {
		Ignored,    // no session was active; the caller should handle the tap normally
		Applied,    // legal target chosen; session is over, chosen() holds it
		Cancelled,  // tapped something that was not a legal target; session is over
	};

	void begin(const std::string& powerId, TargetKind kind, std::vector<PowerTarget> targets);
	TapResult tap(PowerTarget t);
	void cancel();

	bool active() const { return m_active; }
	const std::string& powerId() const { return m_powerId; }
	TargetKind targetKind() const { return m_kind; }
	const std::vector<PowerTarget>& targets() const { return m_targets; }
	// Valid only immediately after tap() returned Applied.
	PowerTarget chosen() const { return m_chosen; }
	bool accepts(PowerTarget t) const;

private:
	bool m_active = false;
	std::string m_powerId;
	TargetKind m_kind = TargetKind::None;
	std::vector<PowerTarget> m_targets;
	PowerTarget m_chosen;
};

}  // namespace lm
