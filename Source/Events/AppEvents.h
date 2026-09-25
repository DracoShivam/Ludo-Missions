#pragma once

#include <string>

namespace lm {

struct ConfigReloaded {
	static constexpr const char* NAME = "lm.app.configReloaded";
	int errors = 0;
};
struct MissionsReloaded {
	static constexpr const char* NAME = "lm.app.missionsReloaded";
	int count = 0;
	int errors = 0;
	int warnings = 0;
};
struct DebugStateChanged {
	static constexpr const char* NAME = "lm.app.debugStateChanged";
	int forcedRoll = 0;
	bool fastBots = false;
	std::string forcedMission;
	bool hasForcedMission = false;  // true when forcedMission field is meaningful (sent by MissionController)
};

}  // namespace lm
