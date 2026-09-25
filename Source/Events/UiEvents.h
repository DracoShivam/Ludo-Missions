#pragma once

// View -> Controller intents. Views publish these; they never call controllers directly.
namespace lm {

struct UiLobbyReady {
	static constexpr const char* NAME = "lm.ui.lobbyReady";
};
struct UiPlayTapped {
	static constexpr const char* NAME = "lm.ui.playTapped";
};
struct UiGameSceneReady {
	static constexpr const char* NAME = "lm.ui.gameSceneReady";
};
struct UiGameSceneExiting {
	static constexpr const char* NAME = "lm.ui.gameSceneExiting";
};
struct UiRollDiceTapped {
	static constexpr const char* NAME = "lm.ui.rollDiceTapped";
};
struct UiTokenTapped {
	static constexpr const char* NAME = "lm.ui.tokenTapped";
	int player = -1;
	int token = -1;
};
struct UiRollChosen {
	static constexpr const char* NAME = "lm.ui.rollChosen";
	int player = -1;
	int token = -1;
	int value = 0;
};
struct UiResultClosed {
	static constexpr const char* NAME = "lm.ui.resultClosed";
	bool playAgain = false;
};

}  // namespace lm
