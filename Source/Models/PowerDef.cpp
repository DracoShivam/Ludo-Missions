#include "Models/PowerDef.h"

namespace lm {

const char* powerTierName(PowerTier tier) {
	switch (tier) {
		case PowerTier::Common: return "common";
		case PowerTier::Rare: return "rare";
		case PowerTier::Epic: return "epic";
	}
	return "common";
}

bool powerTierFromString(const std::string& s, PowerTier& out) {
	if (s == "common") { out = PowerTier::Common; return true; }
	if (s == "rare") { out = PowerTier::Rare; return true; }
	if (s == "epic") { out = PowerTier::Epic; return true; }
	return false;
}

}  // namespace lm
