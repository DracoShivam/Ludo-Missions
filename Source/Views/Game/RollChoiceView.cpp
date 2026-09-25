#include "Views/Game/RollChoiceView.h"

#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "ui/CocosGUI.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

RollChoiceView* RollChoiceView::create(int player, int token, const std::vector<int>& values) {
	auto* v = new (std::nothrow) RollChoiceView();
	if (v && v->initWith(player, token, values)) {
		v->autorelease();
		return v;
	}
	AX_SAFE_DELETE(v);
	return nullptr;
}

bool RollChoiceView::initWith(int player, int token, const std::vector<int>& values) {
	if (!Node::init()) {
		return false;
	}
	const float chip = 52.f;
	float w = values.size() * (chip + 8) + 8;
	auto* bg = ui::makePanel(ax::Size(w, chip + 16), ax::Color3B(30, 30, 40), 230);
	addChild(bg);
	float x = -w / 2 + 8 + chip / 2;
	for (int v : values) {
		auto* b = ax::ui::Button::create(v >= 1 && v <= 6 ? ui::diceFace(v) : ui::IMG_PANEL);
		b->setScale(chip / 200.f);
		b->setPosition(ax::Vec2(x, 0));
		b->setPressedActionEnabled(true);
		b->addClickEventListener([player, token, v](ax::Object*) {
			UiRollChosen e;
			e.player = player;
			e.token = token;
			e.value = v;
			EventBus::publish(e);
		});
		addChild(b);
		x += chip + 8;
	}
	setScale(0.2f);
	runAction(ax::EaseBackOut::create(ax::ScaleTo::create(0.15f, 1.f)));
	return true;
}

}  // namespace lm
