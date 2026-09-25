#pragma once

#include <string>
#include <vector>

#include "Models/GameConfig.h"

namespace lm {

struct ConfigParseResult {
	GameConfig config;  // always usable: missing keys fall back to defaults
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

ConfigParseResult parseGameConfig(const std::string& jsonText);

}  // namespace lm
