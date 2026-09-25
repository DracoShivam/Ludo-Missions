#include "Controllers/ConfigController.h"

#include <fstream>
#include <sstream>

#include "axmol.h"
#include "Controllers/Logic/ConfigParser.h"
#include "Events/AppEvents.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Utils/Log.h"

namespace lm {

ConfigController* ConfigController::sharedController() {
	static ConfigController* s_instance = new ConfigController();
	return s_instance;
}

void ConfigController::init() {
	load();
	EventBus::subscribe<DebugReloadConfig>(this, [this](const DebugReloadConfig&) { load(); });
}

std::string ConfigController::readText(const std::string& relPath) {
#if defined(LM_DEV) && LM_DEV
	std::ifstream in(std::string(LM_SOURCE_CONTENT_DIR) + relPath);
	if (in) {
		std::stringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}
#endif
	return ax::FileUtils::getInstance()->getStringFromFile(relPath);
}

void ConfigController::load() {
	std::string text = readText("config/game_config.json");
	if (text.empty()) {
		LM_LOG_ERROR("config/game_config.json not found; using defaults");
		m_config = GameConfig{};
		EventBus::publish(ConfigReloaded{1});
		return;
	}
	auto result = parseGameConfig(text);
	for (auto& e : result.errors) LM_LOG_ERROR("%s", e.c_str());
	for (auto& w : result.warnings) LM_LOG_WARN("%s", w.c_str());
	m_config = result.config;
	LM_LOG("game_config loaded (%d errors, %d warnings)", (int) result.errors.size(), (int) result.warnings.size());
	EventBus::publish(ConfigReloaded{(int) result.errors.size()});
}

}  // namespace lm
