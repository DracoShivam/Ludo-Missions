#pragma once

#include <map>
#include <string>
#include <vector>

namespace lm {

struct RulesConfig {
	bool threeSixesForfeit = true;
	bool captureBonusRoll = true;
	bool finishBonusRoll = true;
};

struct TimingConfig {
	float diceRollAnim = 0.6f;
	float tokenStep = 0.18f;
	float captureAnim = 0.45f;
	float botThinkDelay = 0.6f;
	float autoMoveDelay = 0.35f;
	float turnGap = 0.35f;
	float resultPopupDelay = 1.5f;
	float fastBotsMultiplier = 0.25f;
};

struct PlayersConfig {
	int humanColor = 0;
	std::vector<std::string> names{"You", "Bot Green", "Bot Yellow", "Bot Blue"};
};

struct MissionSettings {
	bool enabled = true;
	std::string file = "config/missions.json";
	int maxActive = 3;
	int offersPerTurn = 2;
	int offersPerTurnStart = 1;
	int offersPerAfterRoll = 1;
	int defaultCooldownTurns = 3;
};

struct DifficultyConfig {
	double startCenter = 0.5;
	double halfWidth = 0.15;
	double stepOnComplete = -0.05;
	double stepOnFail = 0.05;
	double min = 0.2;
	double max = 0.85;
	double behindBias = 0.1;
	bool persist = true;
};

struct UtilityConfig {
	double timelyBonus = 0.5;
	double noveltyPower = 1.0;
	double repeatPenalty = 0.5;
	double temperature = 0.5;
	double minUtility = 0.05;
};

struct DirectorConfig {
	bool enabled = true;
	int astarMaxExpansions = 4000;
	int rollouts = 96;
	int simBudgetMs = 12;  // 0 = no time limit (tests)
	DifficultyConfig difficulty;
	UtilityConfig utility;
};

struct DebugConfig {
	unsigned int rngSeed = 0;  // 0 = random
	bool fastBots = false;
	std::string forcedMission;
	std::vector<std::vector<int>> startProgress;  // empty or 4x4
};

struct GameConfig {
	RulesConfig rules;
	PlayersConfig players;
	TimingConfig timing;
	MissionSettings missions;
	DirectorConfig director;
	DebugConfig debug;
};

}  // namespace lm
