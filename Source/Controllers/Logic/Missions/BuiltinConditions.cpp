#include <memory>
#include <vector>

#include "Controllers/Logic/BoardQueries.h"
#include "Controllers/Logic/Missions/ConditionRegistry.h"
#include "Controllers/Logic/Rules.h"

namespace lm {

namespace {

bool compareOp(const std::string& op, int a, int b) {
	if (op == "eq") return a == b;
	if (op == "ne") return a != b;
	if (op == "lt") return a < b;
	if (op == "lte") return a <= b;
	if (op == "gt") return a > b;
	if (op == "gte") return a >= b;
	return false;
}
bool validOp(const std::string& op) {
	return op == "eq" || op == "ne" || op == "lt" || op == "lte" || op == "gt" || op == "gte";
}

struct Always : Condition {
	bool eval(const EvalContext&) const override { return true; }
};

struct AllOf : Condition {
	std::vector<ConditionPtr> kids;
	bool eval(const EvalContext& c) const override {
		for (auto& k : kids)
			if (!k->eval(c)) return false;
		return true;
	}
};

struct AnyOf : Condition {
	std::vector<ConditionPtr> kids;
	bool eval(const EvalContext& c) const override {
		for (auto& k : kids)
			if (k->eval(c)) return true;
		return false;
	}
};

struct Not : Condition {
	ConditionPtr kid;
	bool eval(const EvalContext& c) const override { return !kid->eval(c); }
};

struct EnemyAhead : Condition {
	int mn, mx;
	EnemyAhead(int a, int b) : mn(a), mx(b) {}
	bool eval(const EvalContext& c) const override { return queries::anyEnemyAhead(c.state, c.self, mn, mx); }
};

struct EnemyBehind : Condition {
	int mn, mx;
	EnemyBehind(int a, int b) : mn(a), mx(b) {}
	bool eval(const EvalContext& c) const override { return queries::anyEnemyBehind(c.state, c.self, mn, mx); }
};

struct TokenCount : Condition {
	queries::Zone zone;
	bool enemy;
	std::string op;
	int value;
	bool eval(const EvalContext& c) const override {
		int n = enemy ? queries::countEnemyTokens(c.state, c.self, zone) : queries::countTokens(c.state, c.self, zone);
		return compareOp(op, n, value);
	}
};

struct TokenProgressAtLeast : Condition {
	int value;
	bool eval(const EvalContext& c) const override { return queries::maxProgress(c.state, c.self, true) >= value; }
};

struct SelfTurnNumber : Condition {
	std::string op;
	int value;
	bool eval(const EvalContext& c) const override { return compareOp(op, c.selfTurnIndex, value); }
};

struct CanCaptureNow : Condition {
	bool eval(const EvalContext& c) const override {
		if (c.state.current != c.self || c.state.phase != Phase::AwaitingMove) return false;
		for (const auto& m : rules::legalMoves(c.state, c.self))
			if (m.captures) return true;
		return false;
	}
};

bool compileList(const Spec& s, const ConditionRegistry& reg, std::string& err, std::vector<ConditionPtr>& out) {
	auto it = s.params.lists.find("of");
	if (it == s.params.lists.end() || it->second.empty()) {
		err = "'of' must be a non-empty array of conditions";
		return false;
	}
	for (size_t i = 0; i < it->second.size(); i++) {
		auto c = reg.compile(it->second[i], "of[" + std::to_string(i) + "]", err, reg.currentWarnings);
		if (!c) return false;
		out.push_back(c);
	}
	return true;
}

}  // namespace

void registerBuiltinConditions(ConditionRegistry& reg) {
	reg.add("always", {{}, {}, [](const Spec&, const ConditionRegistry&, std::string&) -> ConditionPtr { return std::make_shared<Always>(); }});
	reg.add("all", {{"of"}, {}, [](const Spec& s, const ConditionRegistry& r, std::string& err) -> ConditionPtr {
		auto c = std::make_shared<AllOf>();
		return compileList(s, r, err, c->kids) ? c : nullptr;
	}});
	reg.add("any", {{"of"}, {}, [](const Spec& s, const ConditionRegistry& r, std::string& err) -> ConditionPtr {
		auto c = std::make_shared<AnyOf>();
		return compileList(s, r, err, c->kids) ? c : nullptr;
	}});
	reg.add("not", {{"cond"}, {}, [](const Spec& s, const ConditionRegistry& r, std::string& err) -> ConditionPtr {
		auto it = s.params.children.find("cond");
		if (it == s.params.children.end()) {
			err = "'cond' must be a condition object";
			return nullptr;
		}
		auto c = std::make_shared<Not>();
		c->kid = r.compile(*it->second, "cond", err, r.currentWarnings);
		return c->kid ? c : nullptr;
	}});
	reg.add("enemyAhead", {{}, {"min", "max"}, [](const Spec& s, const ConditionRegistry&, std::string&) -> ConditionPtr {
		return std::make_shared<EnemyAhead>(s.params.getInt("min", 1), s.params.getInt("max", 6));
	}});
	reg.add("enemyBehind", {{}, {"min", "max"}, [](const Spec& s, const ConditionRegistry&, std::string&) -> ConditionPtr {
		return std::make_shared<EnemyBehind>(s.params.getInt("min", 1), s.params.getInt("max", 6));
	}});
	reg.add("tokenCount", {{"zone", "op", "value"}, {"who"}, [](const Spec& s, const ConditionRegistry&, std::string& err) -> ConditionPtr {
		auto c = std::make_shared<TokenCount>();
		std::string zone = s.params.getString("zone", "");
		if (zone == "yard") c->zone = queries::Zone::Yard;
		else if (zone == "track") c->zone = queries::Zone::Track;
		else if (zone == "homeLane") c->zone = queries::Zone::HomeLane;
		else if (zone == "finished") c->zone = queries::Zone::Finished;
		else if (zone == "outOfYard") c->zone = queries::Zone::OutOfYard;
		else {
			err = "zone must be yard|track|homeLane|finished|outOfYard";
			return nullptr;
		}
		std::string who = s.params.getString("who", "self");
		if (who != "self" && who != "enemy") {
			err = "who must be self|enemy";
			return nullptr;
		}
		c->enemy = who == "enemy";
		c->op = s.params.getString("op", "");
		if (!validOp(c->op)) {
			err = "op must be eq|ne|lt|lte|gt|gte";
			return nullptr;
		}
		c->value = s.params.getInt("value", 0);
		return c;
	}});
	reg.add("tokenProgressAtLeast", {{"value"}, {}, [](const Spec& s, const ConditionRegistry&, std::string&) -> ConditionPtr {
		auto c = std::make_shared<TokenProgressAtLeast>();
		c->value = s.params.getInt("value", 0);
		return c;
	}});
	reg.add("selfTurnNumber", {{"op", "value"}, {}, [](const Spec& s, const ConditionRegistry&, std::string& err) -> ConditionPtr {
		auto c = std::make_shared<SelfTurnNumber>();
		c->op = s.params.getString("op", "");
		if (!validOp(c->op)) {
			err = "op must be eq|ne|lt|lte|gt|gte";
			return nullptr;
		}
		c->value = s.params.getInt("value", 0);
		return c;
	}});
	reg.add("canCaptureNow", {{}, {}, [](const Spec&, const ConditionRegistry&, std::string&) -> ConditionPtr { return std::make_shared<CanCaptureNow>(); }});
}

}  // namespace lm
