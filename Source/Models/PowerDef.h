#pragma once

#include <string>

#include "Models/Params.h"

namespace lm {

// Rarity. Which tier a mission pays is derived from its measured completion probability at
// offer time, so retuning a mission moves its reward automatically -- there is no second
// number to keep in sync.
enum class PowerTier { Common = 0, Rare = 1, Epic = 2 };

// What the player must tap after arming a power. Drives both the legal-target search and
// what the board highlights, so the two cannot disagree.
enum class TargetKind { None = 0, OwnToken, OpponentToken, OpponentPlayer };

// One power exactly as authored in powers.json. Pure data: no axmol, no behaviour.
struct PowerDef {
	std::string id;
	std::string title;   // shown on the tray chip
	std::string desc;    // one line, shown when armed
	PowerTier tier = PowerTier::Common;
	Spec effect;         // {"type": "diceDelta", "delta": 3, "who": "self"}
	bool enabled = true;
	int weight = 10;     // relative draw weight within its tier
};

const char* powerTierName(PowerTier tier);
bool powerTierFromString(const std::string& s, PowerTier& out);

}  // namespace lm
