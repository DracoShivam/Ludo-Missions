#include "Views/Game/PlayerPanelView.h"

#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"
#include "Views/Game/DiceView.h"

namespace lm {

PlayerPanelView* PlayerPanelView::create(int player, bool diceOnRight) {
	auto* v = new (std::nothrow) PlayerPanelView();
	if (v && v->initWith(player, diceOnRight)) {
		v->autorelease();
		return v;
	}
	AX_SAFE_DELETE(v);
	return nullptr;
}

bool PlayerPanelView::initWith(int player, bool diceOnRight) {
	if (!Node::init()) {
		return false;
	}
	m_player = player;
	float w = ui::PANEL_W, h = ui::PANEL_H;
	m_glow = ui::makePanel(ax::Size(w + 12, h + 12), ui::playerColor3B(player));
	m_glow->setVisible(false);
	addChild(m_glow);
	m_bg = ui::makePanel(ax::Size(w, h), ui::PANEL_DARK, 235);
	addChild(m_bg);

	float side = diceOnRight ? 1.f : -1.f;
	auto* dot = ax::DrawNode::create();
	dot->drawSolidCircle(ax::Vec2::ZERO, 14, 0, 24, ui::playerColor4B(player));
	dot->setPosition(-side * (w / 2 - 30), 0);
	addChild(dot);

	m_name = ui::makeLabel("", 24);
	m_name->setPosition(-side * 40, 12);
	addChild(m_name);

	m_chips = ax::Node::create();
	m_chips->setPosition(-side * 40, -18);
	addChild(m_chips);

	m_dice = ax::utils::createInstance<DiceView>();
	m_dice->setPosition(side * (w / 2 - 48), 0);
	addChild(m_dice);
	return true;
}

void PlayerPanelView::initListeners() {
	EventBus::subscribe<MatchSnapshot>(this, [this](const MatchSnapshot& s) {
		m_timing = s.timing;
		if (m_player < (int) s.names.size()) m_name->setString(s.names[m_player]);
		setActive(s.state.current == m_player);
		setChips({});
	});
	EventBus::subscribe<GameEventMsg>(this, [this](const GameEventMsg& m) {
		const GameEvent& e = m.event;
		if (e.type == GameEventType::TURN_STARTED) {
			setActive(e.player == m_player);
			if (e.player == m_player) setChips({});
		} else if (e.type == GameEventType::DICE_ROLLED && e.player == m_player) {
			m_dice->roll(e.value, m_timing.diceRollAnim * m.animScale);
		} else if (e.type == GameEventType::MATCH_ENDED) {
			m_dice->setTappable(false);
		}
	});
	EventBus::subscribe<AwaitingRollMsg>(this, [this](const AwaitingRollMsg& m) { m_dice->setTappable(m.isHuman && m.player == m_player); });
	EventBus::subscribe<PendingRollsChanged>(this, [this](const PendingRollsChanged& m) {
		if (m.player == m_player) setChips(m.rolls);
	});
}

void PlayerPanelView::setActive(bool on) {
	m_glow->stopAllActions();
	m_glow->setVisible(on);
	if (on) {
		m_glow->runAction(ax::RepeatForever::create(ax::Sequence::create(ax::FadeTo::create(0.5f, 120), ax::FadeTo::create(0.5f, 255), nullptr)));
	}
	m_bg->setOpacity(on ? 255 : 200);
	if (!on) m_dice->setTappable(false);
}

void PlayerPanelView::setChips(const std::vector<int>& rolls) {
	m_chips->removeAllChildren();
	float x = -((float) rolls.size() - 1) * 15.f;
	for (int v : rolls) {
		auto* s = ax::Sprite::create(ui::diceFace(v));
		s->setScale(26.f / 200.f);
		s->setPosition(x, 0);
		m_chips->addChild(s);
		x += 30.f;
	}
}

}  // namespace lm
