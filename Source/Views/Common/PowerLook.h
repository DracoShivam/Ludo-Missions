#pragma once

#include "axmol.h"

// Tier styling, shared by the mission card and the power tray so a reward promised in one place
// and handed over in another reads as the same thing. Views only: no catalogue, no controller.
namespace lm::ui {

// Saturated enough to survive being drawn on the dark board ground. Pastels wash out to grey here.
inline ax::Color3B powerTierColor(int tier) {
	switch (tier) {
		case 2: return ax::Color3B(214, 104, 66);   // epic   - amber/orange
		case 1: return ax::Color3B(62, 130, 214);   // rare   - blue
		default: return ax::Color3B(60, 172, 102);  // common - green
	}
}

// An unusable chip has to stay recognisably ITS colour while reading as off. Fading it toward
// transparency instead turns every tier into the same grey against the dark panel behind it, which
// is exactly what made the tray look colourless.
inline ax::Color3B powerTierColorDim(int tier) {
	ax::Color3B c = powerTierColor(tier);
	const ax::Color3B slate(58, 66, 92);
	auto mix = [](uint8_t a, uint8_t b) { return (uint8_t) ((a * 42 + b * 58) / 100); };
	return ax::Color3B(mix(c.r, slate.r), mix(c.g, slate.g), mix(c.b, slate.b));
}

inline const char* powerTierName(int tier) {
	switch (tier) {
		case 2: return "EPIC";
		case 1: return "RARE";
		default: return "COMMON";
	}
}

}  // namespace lm::ui
