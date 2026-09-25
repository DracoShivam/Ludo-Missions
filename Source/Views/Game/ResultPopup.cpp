#include "Views/Game/ResultPopup.h"

#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

ResultPopup* ResultPopup::create(const ResultData& data) {
	auto* v = new (std::nothrow) ResultPopup();
	if (v && v->initWith(data)) {
		v->autorelease();
		return v;
	}
	AX_SAFE_DELETE(v);
	return nullptr;
}

bool ResultPopup::initWith(const ResultData& d) {
	if (!Node::init()) {
		return false;
	}
	auto vo = ui::visibleOrigin();
	auto vs = ui::visibleSize();
	auto* dim = ax::LayerColor::create(ax::Color4B(0, 0, 0, 170));
	addChild(dim);
	auto* swallow = ax::EventListenerTouchOneByOne::create();
	swallow->setSwallowTouches(true);
	swallow->onTouchBegan = [](ax::Touch*, ax::Event*) { return true; };
	_eventDispatcher->addEventListenerWithSceneGraphPriority(swallow, dim);

	ax::Vec2 c(vo.x + vs.width / 2, vo.y + vs.height / 2);
	auto* panel = ui::makePanel(ax::Size(560, 640), ui::PANEL_DARK);
	panel->setPosition(c);
	addChild(panel);

	bool won = !d.ranking.empty() && d.ranking[0] == d.self;
	auto* title = ax::Label::createWithTTF(won ? "YOU WIN!" : "GAME OVER", ui::FONT_TITLE, 64);
	title->setPosition(c + ax::Vec2(0, 250));
	addChild(title);

	for (size_t i = 0; i < d.ranking.size(); i++) {
		int p = d.ranking[i];
		float y = 160 - (float) i * 52;
		auto* dot = ax::DrawNode::create();
		dot->drawSolidCircle(ax::Vec2::ZERO, 14, 0, 24, ui::playerColor4B(p));
		dot->setPosition(c + ax::Vec2(-170, y));
		addChild(dot);
		std::string name = p < (int) d.names.size() ? d.names[p] : "?";
		auto* l = ui::makeLabel(std::to_string(i + 1) + ".  " + name, 28, p == d.self);
		l->setAnchorPoint(ax::Vec2(0, 0.5f));
		l->setPosition(c + ax::Vec2(-140, y));
		addChild(l);
	}
	auto* m = ui::makeLabel("Missions completed: " + std::to_string(d.missionsCompleted), 26, false);
	m->setPosition(c + ax::Vec2(0, -90));
	addChild(m);
	auto* coins = ui::makeLabel("Coins earned: +" + std::to_string(d.coinsEarned), 30, true, ax::Color3B(255, 210, 60));
	coins->setPosition(c + ax::Vec2(0, -135));
	addChild(coins);

	auto* again = ui::makeButton("PLAY AGAIN", ax::Size(240, 80), [] { EventBus::publish(UiResultClosed{true}); });
	again->setPosition(c + ax::Vec2(-130, -240));
	addChild(again);
	auto* lobby = ui::makeButton("LOBBY", ax::Size(200, 80), [] { EventBus::publish(UiResultClosed{false}); }, ax::Color3B(90, 110, 170));
	lobby->setPosition(c + ax::Vec2(140, -240));
	addChild(lobby);

	setScale(0.8f);
	runAction(ax::EaseBackOut::create(ax::ScaleTo::create(0.25f, 1.f)));
	return true;
}

}  // namespace lm
