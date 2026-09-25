#include "Controllers/SceneController.h"

#include <cstdlib>

#include "axmol.h"
#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Views/Game/GameScene.h"
#include "Views/Lobby/LobbyScene.h"

namespace lm {

SceneController* SceneController::sharedController() {
	static SceneController* s_instance = new SceneController();
	return s_instance;
}

void SceneController::init() {
	EventBus::subscribe<UiPlayTapped>(this, [this](const UiPlayTapped&) { goToGame(); });
	EventBus::subscribe<UiResultClosed>(this, [this](const UiResultClosed& e) { e.playAgain ? goToGame() : goToLobby(); });
}

void SceneController::runLobby() {
#if defined(LM_DEV) && LM_DEV
	const char* ap = std::getenv("LM_AUTOPLAY");
	if (ap && ap[0] == '1') {
		ax::Director::getInstance()->runWithScene(ax::utils::createInstance<GameScene>());
		return;
	}
#endif
	ax::Director::getInstance()->runWithScene(ax::utils::createInstance<LobbyScene>());
}

void SceneController::goToLobby() {
	ax::Director::getInstance()->replaceScene(ax::TransitionFade::create(0.25f, ax::utils::createInstance<LobbyScene>()));
}

void SceneController::goToGame() {
	ax::Director::getInstance()->replaceScene(ax::TransitionFade::create(0.25f, ax::utils::createInstance<GameScene>()));
}

}  // namespace lm
