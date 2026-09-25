#include "Views/Game/TokenView.h"

#include "Views/Common/UiConfig.h"

namespace lm {

TokenView* TokenView::create(int player, int token) {
	auto* t = new (std::nothrow) TokenView();
	if (t && t->initWith(player, token)) {
		t->autorelease();
		return t;
	}
	AX_SAFE_DELETE(t);
	return nullptr;
}

bool TokenView::initWith(int player, int token) {
	if (!Node::init()) {
		return false;
	}
	m_player = player;
	m_token = token;
	setContentSize(ax::Size(ui::CELL, ui::CELL));
	setAnchorPoint(ax::Vec2(0.5f, 0.5f));
	setIgnoreAnchorPointForPosition(false);
	ax::Vec2 c(ui::CELL / 2, ui::CELL / 2);

	m_shine = ax::Sprite::create(ui::IMG_TOKEN_SHINE);
	m_shine->setScale(73.f / 120.f);
	m_shine->setPosition(c + ax::Vec2(0, 5));
	m_shine->setVisible(false);
	addChild(m_shine);

	auto* base = ax::Sprite::create(ui::IMG_TOKEN_BASE);
	base->setScale(51.f / 85.f);
	base->setPosition(c + ax::Vec2(0, 7));
	addChild(base);

	auto* color = ax::Sprite::create(ui::IMG_TOKEN_COLOR);
	color->setScale(41.f / 71.f);
	color->setPosition(c + ax::Vec2(0, 10));
	color->setColor(ui::playerColor3B(player));
	addChild(color);
	return true;
}

void TokenView::setHighlighted(bool on, bool danger) {
	if (on == m_highlighted && danger == m_danger) {
		return;
	}
	m_highlighted = on;
	m_danger = danger;
	m_shine->stopAllActions();
	m_shine->setVisible(on);
	m_shine->setColor(danger ? ax::Color3B(255, 120, 110) : ax::Color3B::WHITE);
	if (on) {
		if (danger) {
			// Pulse rather than rotate: a target reads as urgent, a movable token as available.
			m_shine->setScale(1.f);
			m_shine->runAction(ax::RepeatForever::create(
				ax::Sequence::create(ax::ScaleTo::create(0.35f, 1.25f), ax::ScaleTo::create(0.35f, 1.0f), nullptr)));
		} else {
			m_shine->setScale(1.f);
			m_shine->runAction(ax::RepeatForever::create(ax::RotateBy::create(1.0f, 360.f)));
		}
	} else {
		m_shine->setScale(1.f);
		m_shine->setRotation(0.f);
	}
}

bool TokenView::hitTest(const ax::Vec2& parentPoint) const {
	ax::Rect box = getBoundingBox();
	box.origin -= ax::Vec2(6, 6);
	box.size = box.size + ax::Size(12, 12);
	return box.containsPoint(parentPoint);
}

}  // namespace lm
