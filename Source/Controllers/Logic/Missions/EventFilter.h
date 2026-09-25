#pragma once

#include <string>
#include <vector>

#include "Models/GameEvent.h"
#include "Models/Params.h"

namespace lm {

// Compiled "where" object: every entry must match. Player fields accept roles "self" | "enemy" | "any".
class EventFilter {
public:
	// Returns false and fills err for unknown fields / bad operators.
	static bool compile(const Params& where, EventFilter& out, std::string& err);
	bool matches(const GameEvent& e, int self) const;
	// True if every match requires an enemy actor (player==enemy, or victimPlayer==self). Used by the A* frozen-bots shortcut.
	bool onlyEnemyCaused() const;

private:
	enum class Op { Eq, Ne, Lt, Lte, Gt, Gte, In, RoleSelf, RoleEnemy, RoleAny };
	struct Clause {
		std::string field;
		Op op;
		std::vector<int> values;
	};
	std::vector<Clause> m_clauses;
};

}  // namespace lm
