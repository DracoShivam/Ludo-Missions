#include "Controllers/Logic/Missions/ConditionRegistry.h"

#include <algorithm>

namespace lm {

ConditionPtr ConditionRegistry::compile(const Spec& spec, const std::string& path, std::string& err, std::vector<std::string>* warnings) const {
	auto it = m_entries.find(spec.type);
	if (spec.type.empty()) {
		err = path + ": missing \"type\"";
		return nullptr;
	}
	if (it == m_entries.end()) {
		err = path + ": unknown condition type '" + spec.type + "'";
		return nullptr;
	}
	const Entry& e = it->second;
	for (const auto& r : e.required) {
		if (!spec.params.has(r)) {
			err = path + " (" + spec.type + "): missing required param '" + r + "'";
			return nullptr;
		}
	}
	if (warnings) {
		for (const auto& k : spec.params.keys()) {
			bool known = std::find(e.required.begin(), e.required.end(), k) != e.required.end() ||
						 std::find(e.optional.begin(), e.optional.end(), k) != e.optional.end() || k == "_note";
			if (!known) warnings->push_back(path + " (" + spec.type + "): unknown param '" + k + "' ignored");
		}
	}
	std::string inner;
	auto* saved = currentWarnings;
	currentWarnings = warnings;
	ConditionPtr c = e.make(spec, *this, inner);
	currentWarnings = saved;
	if (!c) {
		err = path + " (" + spec.type + "): " + inner;
	}
	return c;
}

}  // namespace lm
