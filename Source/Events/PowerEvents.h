#pragma once

#include <string>
#include <vector>

#include "Events/GameEvents.h"
#include "Models/PowerDef.h"

namespace lm {

// One chip in the tray. Everything the view needs to draw it, so Views never reach into a
// controller to ask a question (AGENTS.md: Views must not include Controllers/).
struct PowerChip {
	std::string id;
	std::string title;
	std::string desc;
	PowerTier tier = PowerTier::Common;
	int count = 0;
	// Held, but nothing to aim it at from this board, or not your turn. Shown dimmed rather than
	// hidden: a chip that vanishes and reappears is harder to learn than one that greys out.
	bool usable = false;
};

// The whole tray state in one message. Published by GameController, which is the only place that
// knows both the inventory and the board.
struct PowerState {
	static constexpr const char* NAME = "lm.power.state";
	std::vector<PowerChip> chips;
	std::string armedId;    // empty unless the player is choosing a target
	std::string armedHint;  // "Tap a rival token to send it home"
	bool yourTurn = false;
};

// Inventory changed. Internal to the controller layer; the tray listens to PowerState instead,
// because only GameController can say whether a held power is usable from this board.
struct PowerInventoryChanged {
	static constexpr const char* NAME = "lm.power.inventoryChanged";
};

// A power just fired. Several powers change nothing you can see on the board -- Shield, Skip Turn,
// and a penalty that only bites on someone else's next roll -- so without this they are
// indistinguishable from a power that silently failed.
struct PowerUsedMsg {
	static constexpr const char* NAME = "lm.power.used";
	std::string id;
	std::string title;
	std::string text;  // what it did, in the player's words
	PowerTier tier = PowerTier::Common;
};

struct PowerGranted {
	static constexpr const char* NAME = "lm.power.granted";
	int seat = 0;
	std::string id;
	std::string title;
	PowerTier tier = PowerTier::Common;
	std::string reason;  // "mission:capture_in_3", "home", "bot"
};

}  // namespace lm
