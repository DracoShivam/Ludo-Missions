#pragma once

#include <string>

#include "Models/GameConfig.h"

namespace lm {

// Loads game_config.json. In LM_DEV builds files are read from the SOURCE Content dir so JSON edits apply without a rebuild.
class ConfigController {
public:
	static ConfigController* sharedController();
	void init();
	void load();
	const GameConfig& config() const { return m_config; }
	static std::string readText(const std::string& relPath);

private:
	ConfigController() = default;
	GameConfig m_config;
};

}  // namespace lm
