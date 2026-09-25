#pragma once

// DEV-only intents (published by DebugOverlayView from keyboard input).
namespace lm {

struct DebugForceNextRoll {
	static constexpr const char* NAME = "lm.debug.forceNextRoll";
	int value = 0;  // applies to the HUMAN's next roll only
};
struct DebugReloadConfig {
	static constexpr const char* NAME = "lm.debug.reloadConfig";
};
struct DebugToggleFastBots {
	static constexpr const char* NAME = "lm.debug.toggleFastBots";
};
struct DebugResetCoins {
	static constexpr const char* NAME = "lm.debug.resetCoins";
};
struct DebugCycleForcedMission {
	static constexpr const char* NAME = "lm.debug.cycleForcedMission";
};

}  // namespace lm
