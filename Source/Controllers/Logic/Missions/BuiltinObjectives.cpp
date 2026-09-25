#include <memory>

#include "Controllers/Logic/BoardQueries.h"
#include "Controllers/Logic/Missions/EventFilter.h"
#include "Models/Types.h"
#include "Controllers/Logic/Missions/ObjectiveRegistry.h"

namespace lm {

namespace {

bool isSelfTurnEnd(const GameEvent& e, int self) {
	return e.type == GameEventType::TURN_ENDED && e.player == self;
}

struct EventSpec {
	GameEventType type;
	EventFilter filter;
	bool matches(const GameEvent& e, int self) const { return e.type == type && filter.matches(e, self); }
};

bool compileEventSpec(const Spec& s, EventSpec& out, std::string& err) {
	std::string name = s.params.getString("event", "");
	auto t = gameEventTypeFromString(name);
	if (!t) {
		err = "unknown event '" + name + "'";
		return false;
	}
	out.type = *t;
	auto w = s.params.children.find("where");
	if (w != s.params.children.end()) {
		std::string ferr;
		if (!EventFilter::compile(w->second->params, out.filter, ferr)) {
			err = "where: " + ferr;
			return false;
		}
	}
	return true;
}

// +1 per matching event (or + field value for "sum"); completes at target.
class CountObjective : public Objective {
public:
	CountObjective(std::shared_ptr<const EventSpec> ev, int target, std::string sumField)
		: m_ev(std::move(ev)), m_target(target), m_field(std::move(sumField)) {}
	ObjectiveResult onEvent(const GameEvent& e, const EvalContext& ctx) override {
		if (!m_ev->matches(e, ctx.self)) return {};
		int add = m_field.empty() ? 1 : e.field(m_field).value_or(0);
		if (add == 0) return {};
		m_progress += add;
		return {true, m_progress >= m_target, false};
	}
	int progress() const override { return std::min(m_progress, m_target); }
	int target() const override { return m_target; }
	void setTarget(int t) override {
		if (t > 0) m_target = t;
	}
	bool targetIsTunable() const override { return true; }
	std::unique_ptr<Objective> clone() const override { return std::make_unique<CountObjective>(*this); }
	// Admissible for "finish a token": without capture bonuses a turn moves <= 17 cells (6+6+5; a third 6 forfeits).
	// Only valid when no enemy token is capturable on the track (no bonus rolls possible before the goal).
	int minTurnsHint(const EvalContext& ctx, int) const override {
		if (m_ev->type != GameEventType::TOKEN_FINISHED || !m_field.empty() || m_target - m_progress != 1) return 0;
		if (queries::countEnemyTokens(ctx.state, ctx.self, queries::Zone::Track) > 0) return 0;
		int best = queries::maxProgress(ctx.state, ctx.self, true);
		if (best < 0) best = -1;  // unlock first: yard->start counts as one step of a 6
		int remaining = FINISHED - best;
		return std::max(0, (remaining + 16) / 17 - 1);
	}

private:
	std::shared_ptr<const EventSpec> m_ev;
	int m_target;
	std::string m_field;
	int m_progress = 0;
};

// Consecutive human turns containing >=1 matching event. Updated only at TURN_ENDED(self).
class StreakObjective : public Objective {
public:
	StreakObjective(std::shared_ptr<const EventSpec> ev, int target) : m_ev(std::move(ev)), m_target(target) {}
	ObjectiveResult onEvent(const GameEvent& e, const EvalContext& ctx) override {
		if (m_ev->matches(e, ctx.self) && !m_metThisTurn) {
			m_metThisTurn = true;
			return {true, false, false};
		}
		return {};
	}
	ObjectiveResult onSelfTurnEnded(int turnsLeftAfter) override {
		m_streak = m_metThisTurn ? m_streak + 1 : 0;
		m_metThisTurn = false;
		if (m_streak >= m_target) return {true, true, false};
		if ((m_target - m_streak) > turnsLeftAfter) return {true, false, true};
		return {true, false, false};
	}
	int progress() const override { return m_streak; }
	int target() const override { return m_target; }
	void setTarget(int t) override {
		if (t > 0) m_target = t;
	}
	bool targetIsTunable() const override { return true; }
	std::unique_ptr<Objective> clone() const override { return std::make_unique<StreakObjective>(*this); }
	uint64_t stateKey() const override { return (uint64_t) m_streak * 2 + (m_metThisTurn ? 1 : 0); }
	int minTurnsHint(const EvalContext&, int) const override { return std::max(0, m_target - m_streak - (m_metThisTurn ? 1 : 0)); }

private:
	std::shared_ptr<const EventSpec> m_ev;
	int m_target;
	int m_streak = 0;
	bool m_metThisTurn = false;
};

// Fails on the first matching event; completes when the window closes.
class AvoidObjective : public Objective {
public:
	explicit AvoidObjective(std::shared_ptr<const EventSpec> ev) : m_ev(std::move(ev)) {}
	void begin(const EvalContext&, int turns) override { m_target = turns; }
	ObjectiveResult onEvent(const GameEvent& e, const EvalContext& ctx) override {
		if (m_ev->matches(e, ctx.self)) return {true, false, true};
		return {};
	}
	ObjectiveResult onSelfTurnEnded(int) override {
		m_survived++;
		return {true, false, false};
	}
	bool completesOnWindowEnd() const override { return true; }
	bool completesWhenBotsFrozen() const override { return m_ev->filter.onlyEnemyCaused(); }
	int progress() const override { return m_survived; }
	int target() const override { return m_target; }
	std::unique_ptr<Objective> clone() const override { return std::make_unique<AvoidObjective>(*this); }

private:
	std::shared_ptr<const EventSpec> m_ev;
	int m_target = 1;
	int m_survived = 0;
};

// Completes as soon as the board condition is true after any event.
class StateObjective : public Objective {
public:
	explicit StateObjective(ConditionPtr cond) : m_cond(std::move(cond)) {}
	ObjectiveResult onEvent(const GameEvent&, const EvalContext& ctx) override {
		if (!m_done && m_cond->eval(ctx)) {
			m_done = true;
			return {true, true, false};
		}
		return {};
	}
	bool alreadySatisfied(const EvalContext& ctx) const override { return m_cond->eval(ctx); }
	int progress() const override { return m_done ? 1 : 0; }
	int target() const override { return 1; }
	std::unique_ptr<Objective> clone() const override { return std::make_unique<StateObjective>(*this); }

private:
	ConditionPtr m_cond;
	bool m_done = false;
};

}  // namespace

void registerBuiltinObjectives(ObjectiveRegistry& reg) {
	auto countLike = [](bool isSum) {
		return [isSum](const Spec& s, const ConditionRegistry&, std::string& err) -> ObjectiveFactory {
			auto ev = std::make_shared<EventSpec>();
			if (!compileEventSpec(s, *ev, err)) return nullptr;
			int target = s.params.getInt("target", 1);
			if (target < 1) {
				err = "target must be >= 1";
				return nullptr;
			}
			std::string field;
			if (isSum) {
				field = s.params.getString("field", "");
				if (!GameEvent::isKnownField(field)) {
					err = "unknown field '" + field + "'";
					return nullptr;
				}
			}
			return [ev, target, field]() { return std::unique_ptr<Objective>(new CountObjective(ev, target, field)); };
		};
	};
	reg.add("count", {{"event", "target"}, {"where", "targetRange"}, countLike(false)});
	reg.add("sum", {{"event", "field", "target"}, {"where", "targetRange"}, countLike(true)});
	reg.add("streak", {{"event", "target"}, {"where", "targetRange"}, [](const Spec& s, const ConditionRegistry&, std::string& err) -> ObjectiveFactory {
		auto ev = std::make_shared<EventSpec>();
		if (!compileEventSpec(s, *ev, err)) return nullptr;
		int target = std::max(1, s.params.getInt("target", 1));
		return [ev, target]() { return std::unique_ptr<Objective>(new StreakObjective(ev, target)); };
	}});
	reg.add("avoid", {{"event"}, {"where"}, [](const Spec& s, const ConditionRegistry&, std::string& err) -> ObjectiveFactory {
		auto ev = std::make_shared<EventSpec>();
		if (!compileEventSpec(s, *ev, err)) return nullptr;
		return [ev]() { return std::unique_ptr<Objective>(new AvoidObjective(ev)); };
	}});
	reg.add("state", {{"cond"}, {}, [](const Spec& s, const ConditionRegistry& conds, std::string& err) -> ObjectiveFactory {
		auto it = s.params.children.find("cond");
		if (it == s.params.children.end()) {
			err = "'cond' must be a condition object";
			return nullptr;
		}
		ConditionPtr c = conds.compile(*it->second, "cond", err, conds.currentWarnings);
		if (!c) return nullptr;
		return [c]() { return std::unique_ptr<Objective>(new StateObjective(c)); };
	}});
}

}  // namespace lm
