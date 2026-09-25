#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Controllers/Logic/Missions/Condition.h"
#include "Models/Params.h"

namespace lm {

// type name -> factory. To add a building block: write a Condition class in BuiltinConditions.cpp and add ONE
// reg.add(...) line in registerBuiltinConditions(). No other code changes needed.
class ConditionRegistry {
public:
	using Factory = std::function<ConditionPtr(const Spec&, const ConditionRegistry&, std::string& err)>;
	struct Entry {
		std::vector<std::string> required;
		std::vector<std::string> optional;
		Factory make;
	};
	void add(const std::string& type, Entry entry) { m_entries[type] = std::move(entry); }
	bool has(const std::string& type) const { return m_entries.count(type) > 0; }
	// Validates + builds. Unknown params -> warnings (prefixed with `path`). Returns nullptr + err on failure.
	ConditionPtr compile(const Spec& spec, const std::string& path, std::string& err, std::vector<std::string>* warnings) const;

	mutable std::vector<std::string>* currentWarnings = nullptr;  // used by nested compiles (all/any/not)

private:
	std::map<std::string, Entry> m_entries;
};

void registerBuiltinConditions(ConditionRegistry& reg);

}  // namespace lm
