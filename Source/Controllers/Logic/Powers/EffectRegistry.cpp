#include "Controllers/Logic/Powers/EffectRegistry.h"

#include <algorithm>

namespace lm {

PowerEffectPtr EffectRegistry::compile(const Spec& spec, const std::string& path, std::string& err,
									   std::vector<std::string>* warnings) const {
	if (spec.type.empty()) {
		err = path + ": missing \"type\"";
		return nullptr;
	}
	auto it = m_entries.find(spec.type);
	if (it == m_entries.end()) {
		err = path + ": unknown effect type '" + spec.type + "'";
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
	PowerEffectPtr f = e.make(spec, inner);
	if (!f) err = path + " (" + spec.type + "): " + inner;
	return f;
}

std::vector<std::string> EffectRegistry::kinds() const {
	std::vector<std::string> out;
	for (const auto& [k, _] : m_entries) out.push_back(k);
	return out;
}

}  // namespace lm
