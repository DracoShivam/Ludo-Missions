#pragma once

#include <memory>

#include "Controllers/Logic/Missions/MissionEngine.h"
#include "Controllers/Logic/Rng.h"

namespace lm {

class DirectorStrategy;

// Plug-in: listens to game events, runs the pure MissionEngine, pays coins via WalletController.
// The game never references this class; missions.enabled=false means it subscribes to nothing.
class MissionController {
public:
	static MissionController* sharedController();
	void init();

private:
	MissionController() = default;
	void loadMissions();
	void rebuildStrategy();
	void publish(const std::vector<MissionUpdate>& updates);
	void saveDifficulty();

	MissionEngine m_engine;
	Rng m_rng{0};  // power drops; seeded off the debug seed like the engine
	DirectorStrategy* m_director = nullptr;  // owned by m_engine (null when director disabled)
	int m_completed = 0;
	int m_failed = 0;
	int m_coins = 0;
	bool m_matchActive = false;
	std::string m_forced;
};

}  // namespace lm
