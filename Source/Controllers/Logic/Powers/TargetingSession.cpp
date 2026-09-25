#include "Controllers/Logic/Powers/TargetingSession.h"

#include <algorithm>

namespace lm {

void TargetingSession::begin(const std::string& powerId, TargetKind kind, std::vector<PowerTarget> targets) {
	if (targets.empty()) {
		// Nothing to aim at: refuse to arm rather than arming a mode the player cannot leave by
		// tapping anything meaningful.
		cancel();
		return;
	}
	m_active = true;
	m_powerId = powerId;
	m_kind = kind;
	m_targets = std::move(targets);
	m_chosen = {};
}

bool TargetingSession::accepts(PowerTarget t) const {
	return m_active && std::find(m_targets.begin(), m_targets.end(), t) != m_targets.end();
}

TargetingSession::TapResult TargetingSession::tap(PowerTarget t) {
	if (!m_active) {
		return TapResult::Ignored;
	}
	if (accepts(t)) {
		m_chosen = t;
		m_active = false;
		return TapResult::Applied;
	}
	// The important line. An illegal tap ENDS the session; it is never swallowed. Leaving the
	// mode armed here is precisely what made every later token tap disappear.
	cancel();
	return TapResult::Cancelled;
}

void TargetingSession::cancel() {
	m_active = false;
	m_powerId.clear();
	m_kind = TargetKind::None;
	m_targets.clear();
	m_chosen = {};
}

}  // namespace lm
