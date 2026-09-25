#pragma once

#include <memory>

#include "Controllers/Logic/Missions/Objective.h"

namespace lm {

enum class TrackStatus { Active, Completed, Failed };

// THE single implementation of the mission window/resolution rules (docs/PLAN.md §7.5 steps 1-2).
// Shared by MissionEngine, the A* FeasibilitySearch and the RolloutSimulator so they always agree.
class MissionTracker {
public:
	MissionTracker(std::unique_ptr<Objective> obj, int turns) : m_obj(std::move(obj)), m_turnsLeft(turns) {}
	MissionTracker(const MissionTracker& o) : m_obj(o.m_obj->clone()), m_turnsLeft(o.m_turnsLeft), m_status(o.m_status) {}
	MissionTracker& operator=(const MissionTracker& o) {
		if (this != &o) {
			m_obj = o.m_obj->clone();
			m_turnsLeft = o.m_turnsLeft;
			m_status = o.m_status;
		}
		return *this;
	}
	MissionTracker(MissionTracker&&) = default;
	MissionTracker& operator=(MissionTracker&&) = default;

	void begin(const EvalContext& ctx, int turns) { m_obj->begin(ctx, turns); }

	TrackStatus feed(const GameEvent& e, const EvalContext& ctx, bool* progressed = nullptr) {
		if (m_status != TrackStatus::Active) return m_status;
		bool changed = false;
		ObjectiveResult r = m_obj->onEvent(e, ctx);
		changed |= r.changed;
		if (r.completed) m_status = TrackStatus::Completed;
		else if (r.failed) m_status = TrackStatus::Failed;
		else if (e.type == GameEventType::TURN_ENDED && e.player == ctx.self) {
			m_turnsLeft--;
			ObjectiveResult t = m_obj->onSelfTurnEnded(m_turnsLeft);
			changed = true;  // turnsLeft changed
			if (t.failed) m_status = TrackStatus::Failed;
			else if (t.completed) m_status = TrackStatus::Completed;
			else if (m_turnsLeft <= 0) m_status = m_obj->completesOnWindowEnd() ? TrackStatus::Completed : TrackStatus::Failed;
		}
		if (progressed) *progressed = changed;
		return m_status;
	}

	TrackStatus status() const { return m_status; }
	int turnsLeft() const { return m_turnsLeft; }
	const Objective& objective() const { return *m_obj; }

private:
	std::unique_ptr<Objective> m_obj;
	int m_turnsLeft = 1;
	TrackStatus m_status = TrackStatus::Active;
};

}  // namespace lm
