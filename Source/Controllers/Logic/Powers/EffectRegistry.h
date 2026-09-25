#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "Controllers/Logic/Powers/Effect.h"
#include "Models/Params.h"

namespace lm {

// kind id -> factory. Deliberately the same shape as ObjectiveRegistry: to add a kind of
// power you write one class in BuiltinEffects.cpp and add ONE reg.add(...) line. Adding a
// power that uses an existing kind needs no code at all, only a row in powers.json.
class EffectRegistry {
public:
	using Factory = std::function<PowerEffectPtr(const Spec&, std::string& err)>;
	struct Entry {
		std::vector<std::string> required;
		std::vector<std::string> optional;
		Factory make;
	};
	void add(const std::string& type, Entry entry) { m_entries[type] = std::move(entry); }
	PowerEffectPtr compile(const Spec& spec, const std::string& path, std::string& err, std::vector<std::string>* warnings) const;
	std::vector<std::string> kinds() const;

private:
	std::map<std::string, Entry> m_entries;
};

void registerBuiltinEffects(EffectRegistry& reg);

}  // namespace lm
