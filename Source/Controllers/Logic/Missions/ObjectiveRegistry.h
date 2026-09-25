#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Controllers/Logic/Missions/ConditionRegistry.h"
#include "Controllers/Logic/Missions/Objective.h"
#include "Models/Params.h"

namespace lm {

// type name -> factory producing an ObjectiveFactory (fresh runtime objective per offer).
// To add a shape: write an Objective class in BuiltinObjectives.cpp and add ONE reg.add(...) line.
class ObjectiveRegistry {
public:
	using Factory = std::function<ObjectiveFactory(const Spec&, const ConditionRegistry&, std::string& err)>;
	struct Entry {
		std::vector<std::string> required;
		std::vector<std::string> optional;
		Factory make;
	};
	void add(const std::string& type, Entry entry) { m_entries[type] = std::move(entry); }
	ObjectiveFactory compile(const Spec& spec, const ConditionRegistry& conds, const std::string& path, std::string& err,
							 std::vector<std::string>* warnings) const;

private:
	std::map<std::string, Entry> m_entries;
};

void registerBuiltinObjectives(ObjectiveRegistry& reg);

}  // namespace lm
