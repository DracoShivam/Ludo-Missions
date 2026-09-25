#pragma once

#include <map>
#include <string>
#include <vector>

#include "Controllers/Logic/Powers/PowerCatalog.h"
#include "Models/GameConfig.h"

namespace lm {

// Who holds which powers, for the duration of ONE match.
//
// Knows nothing about missions. Missions are one source of powers exactly as they are the only
// source of coins, and in both cases it is MissionController that hands the reward over -- so the
// dependency runs missions -> powers and never the other way. Turning missions off leaves this
// working with no supply.
//
// Keyed by seat from the start: pass-and-play needs a separate inventory per human, and the
// per-match reset already has the same shape.
class PowerController {
public:
	static PowerController* sharedController();

	void init();         // loads the catalogue; safe to call again on a config reload
	void reloadCatalog();  // re-read powers.json without disturbing what anyone is holding
	void resetMatch();   // clears every seat. Called at match start, NOT at construction.

	bool grant(int seat, const std::string& id, const std::string& reason);
	bool grantTier(int seat, PowerTier tier, const std::string& reason);
	// Pick a power of this tier WITHOUT granting it. Used to resolve a mission's promised reward at
	// offer time, so the card can show it and completion grants exactly what was shown.
	std::string drawId(PowerTier tier);
	bool consume(int seat, const std::string& id);

	int count(int seat, const std::string& id) const;
	int total(int seat) const;
	std::vector<std::string> held(int seat) const;  // ids with count > 0, catalogue order

	const PowerCatalog& catalog() const { return m_catalog; }
	PowerTier tierForProbability(double pComplete) const;

private:
	PowerController() = default;
	PowerCatalog m_catalog;
	PowersConfig m_cfg;
	std::map<int, std::map<std::string, int>> m_held;  // seat -> id -> count
	Rng m_rng{0};
	bool m_subscribed = false;
};

}  // namespace lm
