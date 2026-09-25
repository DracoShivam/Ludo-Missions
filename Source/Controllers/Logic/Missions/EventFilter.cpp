#include "Controllers/Logic/Missions/EventFilter.h"

#include <algorithm>

namespace lm {

static bool toInt(const ParamValue& v, int& out) {
	if (auto* i = std::get_if<int64_t>(&v)) {
		out = (int) *i;
		return true;
	}
	if (auto* d = std::get_if<double>(&v)) {
		out = (int) *d;
		return true;
	}
	return false;
}

static bool isPlayerField(const std::string& f) {
	return f == "player" || f == "victimPlayer";
}

bool EventFilter::compile(const Params& where, EventFilter& out, std::string& err) {
	out.m_clauses.clear();
	for (auto& [field, val] : where.values) {
		if (!GameEvent::isKnownField(field)) {
			err = "unknown field '" + field + "'";
			return false;
		}
		Clause c{field, Op::Eq, {}};
		if (auto* s = std::get_if<std::string>(&val)) {
			if (!isPlayerField(field)) {
				err = "field '" + field + "' does not accept a role string";
				return false;
			}
			if (*s == "self") c.op = Op::RoleSelf;
			else if (*s == "enemy") c.op = Op::RoleEnemy;
			else if (*s == "any") c.op = Op::RoleAny;
			else {
				err = "field '" + field + "': unknown role '" + *s + "' (use self|enemy|any)";
				return false;
			}
		} else {
			int x = 0;
			if (!toInt(val, x)) {
				err = "field '" + field + "' must be a number";
				return false;
			}
			c.values.push_back(x);
		}
		out.m_clauses.push_back(c);
	}
	for (auto& [field, spec] : where.children) {
		if (!GameEvent::isKnownField(field)) {
			err = "unknown field '" + field + "'";
			return false;
		}
		const Params& ops = spec->params;
		if (ops.values.size() + ops.arrays.size() != 1 || !ops.children.empty() || !ops.lists.empty()) {
			err = "field '" + field + "': operator object needs exactly one of eq|ne|lt|lte|gt|gte|in";
			return false;
		}
		Clause c{field, Op::Eq, {}};
		if (!ops.arrays.empty()) {
			auto& [name, arr] = *ops.arrays.begin();
			if (name != "in") {
				err = "field '" + field + "': only 'in' takes an array";
				return false;
			}
			c.op = Op::In;
			for (auto& v : arr) {
				int x = 0;
				if (!toInt(v, x)) {
					err = "field '" + field + "': 'in' values must be numbers";
					return false;
				}
				c.values.push_back(x);
			}
		} else {
			auto& [name, v] = *ops.values.begin();
			static const std::pair<const char*, Op> OPS[] = {{"eq", Op::Eq}, {"ne", Op::Ne}, {"lt", Op::Lt}, {"lte", Op::Lte}, {"gt", Op::Gt}, {"gte", Op::Gte}};
			bool found = false;
			for (auto& [n, op] : OPS) {
				if (name == n) {
					c.op = op;
					found = true;
				}
			}
			int x = 0;
			if (!found || !toInt(v, x)) {
				err = "field '" + field + "': bad operator '" + name + "'";
				return false;
			}
			c.values.push_back(x);
		}
		out.m_clauses.push_back(c);
	}
	if (!where.lists.empty() || !where.arrays.empty()) {
		err = "unexpected array in 'where'";
		return false;
	}
	return true;
}

bool EventFilter::onlyEnemyCaused() const {
	for (const auto& c : m_clauses) {
		if (c.field == "player" && c.op == Op::RoleEnemy) return true;
		if (c.field == "victimPlayer" && c.op == Op::RoleSelf) return true;
	}
	return false;
}

bool EventFilter::matches(const GameEvent& e, int self) const {
	for (const auto& c : m_clauses) {
		int v = *e.field(c.field);
		switch (c.op) {
			case Op::Eq: if (v != c.values[0]) return false; break;
			case Op::Ne: if (v == c.values[0]) return false; break;
			case Op::Lt: if (!(v < c.values[0])) return false; break;
			case Op::Lte: if (!(v <= c.values[0])) return false; break;
			case Op::Gt: if (!(v > c.values[0])) return false; break;
			case Op::Gte: if (!(v >= c.values[0])) return false; break;
			case Op::In: if (std::find(c.values.begin(), c.values.end(), v) == c.values.end()) return false; break;
			case Op::RoleSelf: if (v != self) return false; break;
			case Op::RoleEnemy: if (v < 0 || v == self) return false; break;
			case Op::RoleAny: if (v < 0) return false; break;
		}
	}
	return true;
}

}  // namespace lm
