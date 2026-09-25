#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Controllers/Logic/Powers/EffectRegistry.h"
#include "Controllers/Logic/Rng.h"
#include "Models/PowerDef.h"

namespace lm {

struct CompiledPower {
	PowerDef def;
	PowerEffectPtr effect;
};
using CompiledPowerPtr = std::shared_ptr<const CompiledPower>;

struct PowerParseResult {
	std::vector<CompiledPowerPtr> powers;  // valid + enabled, JSON order
	std::vector<std::string> errors;       // a bad power is skipped, the rest still load
	std::vector<std::string> warnings;
};

// The loaded catalogue. Owns nothing axmol and no match state, so it is fully testable.
class PowerCatalog {
public:
	PowerCatalog();
	PowerParseResult loadFromJson(const std::string& json);

	const CompiledPower* find(const std::string& id) const;
	// Weighted draw within a tier. Falls back to the next tier down when a tier is empty, so a
	// catalogue missing its EPIC rows still rewards rather than silently granting nothing.
	const CompiledPower* draw(PowerTier tier, Rng& rng) const;
	const std::vector<CompiledPowerPtr>& all() const { return m_powers; }
	std::vector<std::string> ids() const;
	const EffectRegistry& effects() const { return m_effects; }

private:
	EffectRegistry m_effects;
	std::vector<CompiledPowerPtr> m_powers;
};

PowerParseResult parsePowers(const std::string& json, const EffectRegistry& effects);

}  // namespace lm
