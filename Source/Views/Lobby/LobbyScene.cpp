#include "Views/Lobby/LobbyScene.h"

#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Views/Common/CoinCounterView.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

bool LobbyScene::init() {
	if (!Scene::init()) {
		return false;
	}
	auto vo = ui::visibleOrigin();
	auto vs = ui::visibleSize();
	addChild(ax::LayerColor::create(ui::BG_COLOR));

	auto* coins = ax::utils::createInstance<CoinCounterView>();
	coins->setPosition(vo.x + vs.width - 110, ui::topY(ui::TOP_BAR_FROM_TOP));
	addChild(coins);

	auto* title = ax::Label::createWithTTF("LUDO\nMISSIONS", ui::FONT_TITLE, 96);
	title->setAlignment(ax::TextHAlignment::CENTER);
	title->setPosition(vo.x + vs.width / 2, vo.y + vs.height * 0.66f);
	addChild(title);

	auto* sub = ui::makeLabel("Complete missions. Earn coins.", 28, false, ax::Color3B(200, 210, 240));
	sub->setPosition(vo.x + vs.width / 2, vo.y + vs.height * 0.52f);
	addChild(sub);

	auto* play = ui::makeButton("PLAY", ax::Size(320, 110), [] { EventBus::publish(UiPlayTapped{}); });
	play->setPosition(ax::Vec2(vo.x + vs.width / 2, vo.y + vs.height * 0.36f));
	addChild(play);

#if defined(LM_DEV) && LM_DEV
	auto* reset = ui::makeButton("reset coins", ax::Size(200, 56), [] { EventBus::publish(DebugResetCoins{}); }, ax::Color3B(120, 120, 140));
	reset->setPosition(ax::Vec2(vo.x + vs.width / 2, vo.y + 80));
	addChild(reset);
#endif
	return true;
}

void LobbyScene::afterEnter() {
	EventBus::publish(UiLobbyReady{});
}

}  // namespace lm
