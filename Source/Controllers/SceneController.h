#pragma once

namespace lm {

// Scene navigation. The ONLY controller allowed to include Views (it is the scene factory).
class SceneController {
public:
	static SceneController* sharedController();
	void init();
	void runLobby();  // first scene (runWithScene)
	void goToLobby();
	void goToGame();

private:
	SceneController() = default;
};

}  // namespace lm
