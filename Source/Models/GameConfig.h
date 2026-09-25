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
	// The player should always have a live mission. When true and nothing is active, the per-turn offer caps, the
	// per-mission cooldown and the Director's minimum-utility gate are each waived for that one offer. Engagement
	// outranks preference: "no offer is better than a bad offer" stops being true once the mission IS the engagement.
	bool alwaysOn = true;
};

struct PowersConfig {
	bool enabled = true;
	std::string file = "config/powers.json";
	// Inventory is per MATCH, not persisted: unspent powers are lost when the match ends. Banking
	// them across games turns an interesting mid-match decision into a loaded opening.
	int maxHeld = 3;       // per power id
	int maxTotalHeld = 4;  // across all powers
	// Tier is derived from the mission's measured completion probability at offer time, so retuning
	// a mission moves its reward with it. P >= common, else P >= rare, else epic.
	double commonAbove = 0.55;
	double rareAbove = 0.30;
	// A token reaching home grants one power at this tier.
	std::string onTokenHomeTier = "rare";
	// Bots have no missions, so a turn timer is the only symmetric supply. Off means the human is
	// strictly stronger, which drifts the mission difficulty model downward over a session.
	bool botsUsePowers = true;
	int botGrantEveryTurns = 12;
	// Share of bot grants drawn from RARE rather than COMMON. Without it bots hold nothing but the
	// two common powers, and since one of those is only spendable after a roll they end up using a
	// single power all game.
	double botRareChance = 0.35;
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
	double minUtility = 0.1;
	double fitFloor = 0.1;  // lets near-certain / near-impossible missions still be served when they are the only fit
	// Hard floor on simulated success. fitFloor means the difficulty term can never reject anything on its own -- a
	// mission the rollouts never completed still scores weight/10 * 0.1 * timely, which clears minUtility on
	// timeliness alone. Without this floor roughly one offer in 25 was something the player could not actually do.
	double minProbability = 0.05;
};

struct DirectorConfig {
	bool enabled = true;
	int astarMaxExpansions = 4000;  // PLAN.md §8. At 300 most searches exhaust the budget and return Unknown, which counts as feasible.
	int rollouts = 96;
	int simBudgetMs = 12;      // Monte Carlo only, measured from the END of the A* stage. 0 = no limit (tests).
	int astarBudgetMs = 5;     // Wall-clock cap on the whole A* stage. Candidates not reached are treated as
	                           // feasible (Unknown), which is the safe direction. 0 = no limit (tests).
	// How many candidates get the expensive treatment (A* + rollouts). Cost per decision is linear in this, so
	// without a cap a growing catalogue silently starves every candidate of samples: round-robin over N
	// candidates needs 16*N playouts before any of them clears MIN_RUNS_UNDER_BUDGET. The shortlist is drawn by
	// weighted sampling on designer weight and novelty, so low-weight missions still get their turn.
	int maxEvaluated = 6;      // 0 = evaluate everything (tests)
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
	PowersConfig powers;
	DebugConfig debug;
};

}  // namespace lm
